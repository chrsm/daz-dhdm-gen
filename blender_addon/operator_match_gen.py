import bpy, os, time, mathutils, json
from . import dll_wrapper
from . import utils
from .operator_common import dhdmGenBaseOperator, MatchedFiles


class GeoMatch:
    def __init__(self, d_vn):
        self.d_vn = d_vn


class GenerateMatching(dhdmGenBaseOperator):
    """Generate matching files for base mesh"""
    bl_idname = "dazdhdmgen.generatematch"
    bl_label = "Generate matching files"

    hd_level_max:   bpy.props.IntProperty(name="Max subdivisions", default=2, min=1, max=5, step=1)

    force_new:  bpy.props.BoolProperty( name="Overwrite all", default=False,
                                        description="Force generation of files for all subdivision levels, overwriting any existing compatible files" )

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        row = layout.row()
        row.prop(self, "hd_level_max")
        row = layout.row()
        row.prop(self, "force_new")

    def execute(self, context):
        t0 = time.perf_counter()
        if not self.check_input(context, check_hd=False, check_dsf=False):
            return {'CANCELLED'}

        r = self.generate_matches(context)
        self.cleanup(context)
        if not r:
            return {'CANCELLED'}
        self.report({'INFO'}, "Matching file/s generated.")
        print("Elapsed: {}".format(time.perf_counter() - t0))
        return {'FINISHED'}

    def create_matching_file(self, gm, level):
        print("Writing matching file...")
        if not os.path.isdir(self.matching_files_dir):
            raise RuntimeError("Directory \"{0}\" not found.".format(self.matching_files_dir))
        filename = "f{0}_div{1}.json".format( self.mfiles.fingerprint, level )
        fp = os.path.join(self.matching_files_dir, filename)
        utils.j_to_json_file(fp, gm.d_vn, with_gzip=True, indent=None)
        print("File \"{0}\" generated.".format(fp))

    def generate_matches(self, context):
        ob_base_copy = utils.copy_object(self.base_ob)
        ob_base_copy.modifiers.clear()
        utils.remove_shape_keys(ob_base_copy)
        utils.delete_uv_layers(ob_base_copy)
        ob_base_copy.vertex_groups.clear()
        ob_base_copy.data.materials.clear()
        ob_base_copy.parent = None
        ob_base_copy.matrix_world.translation = (0, 0, 0)

        f_name = "base"
        fp_base = self.export_ob_obj( ob_base_copy, f_name, apply_modifiers=False )
        outputDirpath = self.create_temporary_subdir()

        missing_levels = None
        if self.force_new:
            missing_levels = [ level for level in range(1, self.hd_level_max+1) ]
        else:
            missing_levels = self.mfiles.get_missing_levels(self.hd_level_max)

        if len(missing_levels) == 0:
            self.report({'INFO'}, "Matching file/s already exist.")
            return False

        mr = utils.create_multires_modifier(ob_base_copy)
        mr.show_viewport = True

        for level in range(1, self.hd_level_max+1):
            utils.make_single_active(ob_base_copy)
            bpy.ops.object.multires_subdivide(modifier=mr.name, mode='CATMULL_CLARK')

            if level not in missing_levels:
                continue
            print("Performing matching for level {0}...".format(level))

            outputFilename = "{0}-div{1}".format(utils.makeValidFilename(self.base_ob.name), level)
            dll_wrapper.execute_in_new_thread( "generate_hd_mesh",
                                               self.gScale, fp_base, level,
                                               outputDirpath, outputFilename )
            hd_dz_ob = utils.import_dll_obj(os.path.join(outputDirpath, outputFilename + ".obj"))

            ds = context.evaluated_depsgraph_get()
            ob_base_copy_eval = ob_base_copy.evaluated_get(ds)

            if len(hd_dz_ob.data.vertices) != len(ob_base_copy_eval.data.vertices):
                raise RuntimeError("Vertex count mismatch.")

            print("Matching vertices by distance...")
            gm = self.create_matching_map_distance( ob_base_copy_eval, hd_dz_ob )
            if gm is None:
                utils.delete_object(hd_dz_ob)
                utils.delete_object(ob_base_copy)
                return False
            print("Done matching vertices.")

            try:
                self.create_matching_file(gm, level)
            except Exception as e:
                utils.delete_object(ob_base_copy)
                self.report({'ERROR'}, "Generation of matching file failed.")
                raise e
            finally:
                utils.delete_object(hd_dz_ob)

        utils.delete_object(ob_base_copy)
        return True

    def create_matching_map_distance(self, hd_mr_ob, hd_dz_ob):
        kd = mathutils.kdtree.KDTree(len(hd_mr_ob.data.vertices))
        for i, v in enumerate(hd_mr_ob.data.vertices):
            kd.insert(v.co, i)
        kd.balance()

        max_dist = 3e-3
        max_non_optimal_n = 50
        non_optimal_n = 0
        d_vn = {}
        for j, v in enumerate(hd_dz_ob.data.vertices):
            co, i, dist = kd.find(v.co)
            if dist > max_dist:
                non_optimal_n += 1
                print("WARNING: vertex matching wasn't optimal (distance = {0}).".format(dist))
                if non_optimal_n > max_non_optimal_n:
                    self.report({'ERROR'}, "Canceled: more than {0} non-optimal matches.".format(max_non_optimal_n))
                    return None
            d_vn[j] = i

        gm = GeoMatch(d_vn)
        return gm

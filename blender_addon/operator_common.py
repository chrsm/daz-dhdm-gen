import bpy, os, re
from mathutils import Vector
from . import utils

class MatchedFiles:
    def __init__(self, ob, files_dir):
        if not os.path.isdir(files_dir):
            raise ValueError("Directory {0} doesn't exist.".format(files_dir))

        self.fingerprint = utils.get_fingerprint(ob)
        self.matched = {}
        for root, dirs, files in os.walk(files_dir):
            for fn in files:
                fn_fingerprint, fn_levels, fn_mrm = utils.get_info_from_filename(fn)
                if (fn_fingerprint is not None) and (fn_fingerprint == self.fingerprint):
                    if fn_mrm not in self.matched:
                        self.matched[fn_mrm] = {}
                    self.matched[fn_mrm][fn_levels] = os.path.join(root, fn)
            break

    def get_suffix(self, base_subdiv_method):
        assert(base_subdiv_method in ('MULTIRES', 'MULTIRES_REC'))
        if base_subdiv_method == 'MULTIRES_REC':
            return "mrr"
        return "mr"

    def get_filepaths(self, max_level, base_subdiv_method):
        mrm = self.get_suffix(base_subdiv_method)
        filepaths = []
        if mrm not in self.matched:
            raise RuntimeError("get_filepaths(): missing matching files")
        for level in range(1, max_level+1):
            fp = self.matched[mrm].get(level)
            if fp is None:
                raise RuntimeError("get_filepaths(): missing matching files")
            filepaths.append(fp)
        return filepaths

    def get_missing_levels(self, max_level, base_subdiv_method):
        mrm = self.get_suffix(base_subdiv_method)
        missing_levels = []
        for level in range(1, max_level+1):
            if (mrm not in self.matched) or (level not in self.matched[mrm]):
                missing_levels.append(level)
        return missing_levels


class dhdmGenBaseOperator(bpy.types.Operator):
    working_dirpath = None
    gScale = None
    base_ob = None
    matching_files_dir = None
    mfiles = None

    temporary_subdirname = "_temporary"
    new_morphs_subdirname = "new_morphs"
    base_ob_copy = None
    cleanup_files = None
    saved_settings = None

    hd_ob = None
    base_subdiv_method = None
    morphed_base_ob = None
    morph_name = None
    dsf_fp = None

    @classmethod
    def poll(cls, context):
        return context.mode == 'OBJECT'

    def invoke(self, context, event):
        return self.execute(context)

    def check_input(self, context, check_hd, check_dsf, check_morph_name):
        scn = context.scene
        addon_props = scn.daz_dhdm_gen

        if addon_props.base_ob not in scn.objects:
            self.report({'ERROR'}, "Base mesh not found in the scene.")
            return False
        base_ob = scn.objects[ addon_props.base_ob ]
        if not base_ob or base_ob.type != 'MESH':
            self.report({'ERROR'}, "Invalid base mesh.")
            return False
        self.base_ob = base_ob

        self.gScale = addon_props.unit_scale

        if check_hd:
            if addon_props.hd_ob not in scn.objects:
                self.report({'ERROR'}, "HD object not found in the scene.")
                return False
            hd_ob = scn.objects[ addon_props.hd_ob ]
            if not hd_ob or hd_ob.type != 'MESH':
                self.report({'ERROR'}, "Invalid HD object.")
                return False
            if len(hd_ob.data.vertices) < len(self.base_ob.data.vertices):
                self.report({'ERROR'}, "HD mesh has fewer vertices than base mesh.")
                return False
            self.morphed_base_ob = None
            if addon_props.base_morphs == 'BASE_MORPHED':
                if addon_props.morphed_base_ob not in scn.objects:
                    self.report({'ERROR'}, "Morphed base mesh not found in the scene.")
                    return False
                morphed_base_ob = scn.objects[ addon_props.morphed_base_ob ]
                if not morphed_base_ob or morphed_base_ob.type != 'MESH':
                    self.report({'ERROR'}, "Invalid morphed base mesh.")
                    return False
                if len(self.base_ob.data.vertices) != len(morphed_base_ob.data.vertices):
                    self.report({'ERROR'}, "Morphed base mesh's vertex count doesn't match base mesh's.")
                    return False
                self.morphed_base_ob = morphed_base_ob
            self.hd_ob = hd_ob
            self.base_subdiv_method = addon_props.base_subdiv_method

        working_dirpath = addon_props.working_dirpath
        if not working_dirpath.strip():
            self.report({'ERROR'}, "Working directory not set.")
            return False
        working_dirpath = os.path.abspath( bpy.path.abspath(working_dirpath) )
        if not os.path.isdir(working_dirpath):
            self.report({'ERROR'}, "Working directory not found.")
            return False
        self.working_dirpath = os.path.join(working_dirpath, utils.makeValidFilename(self.base_ob.name))
        if not os.path.isdir(self.working_dirpath):
            os.mkdir(self.working_dirpath)
        self.cleanup_files = context.preferences.addons[__package__].preferences.delete_temporary_files

        matching_files_dir = os.path.abspath( bpy.path.abspath(addon_props.matching_files_dir) )
        if not os.path.isdir(matching_files_dir):
            self.report({'ERROR'}, "Invalid matching files directory.")
            return False
        self.matching_files_dir = matching_files_dir
        self.mfiles = MatchedFiles(self.base_ob, self.matching_files_dir)

        self.dsf_fp = None
        if check_dsf and (not addon_props.only_dhdm):
            dsf_fp = os.path.abspath( bpy.path.abspath( addon_props.dsf_file_template ) )
            if not utils.has_extension( dsf_fp, "dsf" ):
                self.report({'ERROR'}, "Invalid .dsf file.")
                return False
            if not os.path.isfile(dsf_fp):
                self.report({'ERROR'}, ".dsf file not found.")
                return False
            self.dsf_fp = dsf_fp

        self.morph_name = None
        if check_morph_name:
            morph_name = addon_props.morph_name.strip()
            if not re.match(r"^\w+$", morph_name):
                self.report({'ERROR'}, "Morph name given is not valid.")
                return False
            self.morph_name = morph_name

        self.save_settings(context)
        return True

    def check_all_matching_files(self, hd_level):
        missing_levels = self.mfiles.get_missing_levels(hd_level, self.base_subdiv_method)
        if len(missing_levels) > 0:
            self.report({'ERROR'}, "Matching files for levels {0} missing. Try generating them.".format(missing_levels))
            return False
        return True

    def save_settings(self, context):
        s = {}
        if context.scene.render.use_simplify:
            s["simplify"] = True
            context.scene.render.use_simplify = False
        self.saved_settings = s

    def restore_settings(self, context):
        if self.saved_settings is None:
            return
        if "simplify" in self.saved_settings:
            context.scene.render.use_simplify = self.saved_settings["simplify"]
        self.saved_settings = None

    def create_subdir(self, subdirpath):
        if not os.path.isdir(subdirpath):
            os.mkdir(subdirpath)
        return subdirpath

    def create_temporary_subdir(self):
        return self.create_subdir(self.get_temporary_subdir())

    def get_temporary_subdir(self):
        return os.path.join(self.working_dirpath, self.temporary_subdirname)

    def get_new_mophs_subdir(self):
        return os.path.join(self.working_dirpath, self.new_morphs_subdirname)

    def create_new_morphs_subdir(self):
        return self.create_subdir(self.get_new_mophs_subdir())

    def export_ob_obj( self, ob, name, apply_modifiers ):
        tmp_dir = self.create_temporary_subdir()
        fp_base = os.path.join(tmp_dir, name)
        fp = fp_base + ".obj"

        ob_mw_trans_prev = Vector(ob.matrix_world.translation)
        ob.matrix_world.translation = (0, 0, 0)
        utils.make_single_active(ob)
        bpy.ops.wm.obj_export( filepath=fp, check_existing=False,
                               export_animation=False, apply_modifiers=apply_modifiers,
                               export_eval_mode='DAG_EVAL_VIEWPORT',
                               export_selected_objects=True, export_uv=False,
                               export_normals=False, export_colors=False,
                               export_materials=False, export_pbr_extensions=False,
                               export_triangulated_mesh=False, export_material_groups=False,
                               export_vertex_groups=False, export_smooth_groups=False )
        ob.matrix_world.translation = ob_mw_trans_prev
        return fp_base

    def export_ob_dae( self, ob, name, apply_modifiers, with_obj ):
        tmp_dir = self.create_temporary_subdir()
        fp_base = os.path.join(tmp_dir, name)
        fp = fp_base + ".dae"

        utils.make_single_active(ob)
        bpy.ops.wm.collada_export( filepath=fp, check_existing=False,
                                   apply_modifiers = apply_modifiers,
                                   export_mesh_type_selection = 'view',
                                   selected = True,
                                   include_children = False,
                                   include_armatures = False,
                                   include_shapekeys = False,
                                   deform_bones_only = False,
                                   include_animations = False,
                                   include_all_actions = False,
                                   active_uv_only = True,
                                   use_texture_copies = False,
                                   triangulate = False,
                                   use_blender_profile = False,
                                   limit_precision = False,
                                   keep_bind_info = False )

        if with_obj:
            fp = fp_base + ".obj"
            bpy.ops.wm.obj_export( filepath=fp, check_existing=False,
                                   export_animation=False, apply_modifiers=apply_modifiers,
                                   export_eval_mode='DAG_EVAL_VIEWPORT',
                                   export_selected_objects=True, export_uv=False,
                                   export_normals=False, export_colors=False,
                                   export_materials=False, export_pbr_extensions=False,
                                   export_triangulated_mesh=False, export_material_groups=False,
                                   export_vertex_groups=False, export_smooth_groups=False )

        return fp_base

    def get_ob_to_export(self, base_modifiers):
        assert( base_modifiers in {'NONE', 'SK', 'NO_SK', 'NO_ARMATURE', 'ALL'} )
        ob_to_export = None
        apply_modifiers = None

        if base_modifiers == 'NONE':
            ob_to_export = self.base_ob
            apply_modifiers = False
        else:
            self.base_ob_copy = utils.copy_object(self.base_ob)
            ob_to_export = self.base_ob_copy
            apply_modifiers = True
            if base_modifiers == 'SK':
                self.base_ob_copy.modifiers.clear()
            else:
                utils.remove_division_modifiers(self.base_ob_copy)
                if base_modifiers == 'NO_ARMATURE':
                    utils.remove_armature_modifiers(self.base_ob_copy)
                elif base_modifiers == 'NO_SK':
                    utils.remove_shape_keys(self.base_ob_copy)

        return ob_to_export, apply_modifiers

    def export_base_obj( self, context, base_modifiers ):
        ob_to_export, apply_modifiers = self.get_ob_to_export(base_modifiers)
        fp_base = self.export_ob_obj( ob_to_export, "base", apply_modifiers )
        self.cleanup_base_copy()
        return fp_base

    def export_base_dae( self, context, base_modifiers, with_obj ):
        ob_to_export, apply_modifiers = self.get_ob_to_export(base_modifiers)
        fp_base = self.export_ob_dae( ob_to_export, "base", apply_modifiers, with_obj )
        self.cleanup_base_copy()
        return fp_base

    def cleanup_base_copy(self):
        if self.base_ob_copy is not None:
            utils.delete_object(self.base_ob_copy)
            self.base_ob_copy = None

    def cleanup(self, context):
        self.cleanup_base_copy()
        self.restore_settings(context)
        if self.cleanup_files:
            tmp_dir = self.get_temporary_subdir()
            if not os.path.isdir(tmp_dir):
                return
            fp_to_delete = []
            for fn in os.listdir(tmp_dir):
                fp = os.path.join(tmp_dir, fn)
                if os.path.isfile(fp) and utils.has_extension(fn, "obj", "mtl", "dae"):
                    fp_to_delete.append(fp)
            for fp in fp_to_delete:
                os.remove(fp)

    def cleanup_shape_files(self):
        tmp_dir = self.get_temporary_subdir()
        if not os.path.isdir(tmp_dir):
            return
        exp = re.compile(r"shape-\d+\.dae")
        fp_to_delete = []
        for fn in os.listdir(tmp_dir):
            fp = os.path.join(tmp_dir, fn)
            if os.path.isfile(fp) and exp.match(fn):
                fp_to_delete.append(fp)
        for fp in fp_to_delete:
            os.remove(fp)

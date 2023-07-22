import os, time, json, re, bpy
from . import dll_wrapper
from . import utils
from .operator_common import dhdmGenBaseOperator
from mathutils import Vector, Matrix


class GenerateNewMorphFiles(dhdmGenBaseOperator):
    """Generate .dsf and .dhdm files from a daz base mesh with a multiresolution modifier, either already applied or not"""
    bl_idname = "dazdhdmgen.generatenewmorph"
    bl_label = "Generate .dsf/.dhdm files"

    hd_level = None
    subd_m = None
    morph_files_diroutput = None
    morph_files_filename = None

    blend_to_dz_mat = Matrix( [ (1, 0, 0),
                                (0, 0, 1),
                                (0, -1, 0) ] )

    def execute(self, context):
        t0 = time.perf_counter()
        if not self.check_input(context, check_hd=True, check_dsf=True):
            return {'CANCELLED'}
        if not self.get_hd_level():
            return {'CANCELLED'}
        if not self.check_all_matching_files(self.hd_level):
            return {'CANCELLED'}

        self.morph_files_diroutput = self.create_new_morphs_subdir()
        self.morph_files_filename = "prueba0_div{0}".format(self.hd_level).strip()
        # ~ r = self.generate_dsf_file(context)
        # ~ if not r:
            # ~ self.cleanup(context)
            # ~ return {'CANCELLED'}
        r = self.generate_dhdm_file(context)
        self.cleanup(context)
        if not r:
            return {'CANCELLED'}
        self.report({'INFO'}, ".dsf and .dhdm files generated.")
        print("Elapsed: {}".format(time.perf_counter() - t0))
        return {'FINISHED'}


    def get_subdiv_modifier_info(self, ob):
        subd_level = 0
        subd_m = None
        for m in ob.modifiers:
            if m.type in ('SUBSURF', 'MULTIRES'):
                if subd_m is not None:
                    self.report({'ERROR'}, "hd mesh has more than 1 subdivision modifier.")
                    return -1, None
                if m.type == 'SUBSURF':
                    subd_level = m.levels
                else:
                    m.levels = m.total_levels
                    subd_level = m.total_levels
                subd_m = m
        return subd_level, subd_m

    def get_hd_level(self):
        subd_level, subd_m = self.get_subdiv_modifier_info(self.hd_ob)
        if subd_m is None:
            diff = utils.get_subdivision_level(self.base_ob, self.hd_ob)
            if diff <= 0:
                self.report({'ERROR'}, "hd mesh has no multiresolution modifier and is not subdivided.")
                return False
            self.hd_level = diff
            self.subd_m = None
        else:
            if subd_m.type != 'MULTIRES':
                self.report({'ERROR'}, "hd mesh should have multiresolution modifier.")
                return False
            if len(self.hd_ob.data.vertices) != len(self.base_ob.data.vertices):
                self.report({'ERROR'}, "hd mesh's base level's vertex count doesn't match base mesh's.")
                return False
            self.hd_level = subd_level
            self.subd_m = subd_m
        return True

    def get_base_morph_info(self, context):
        ob_base_copy = utils.copy_object(self.base_ob)
        ob_base_copy.modifiers.clear()
        utils.remove_shape_keys(ob_base_copy)
        utils.delete_uv_layers(ob_base_copy)
        ob_base_copy.vertex_groups.clear()
        ob_base_copy.data.materials.clear()
        ob_base_copy.parent = None
        ob_base_copy.matrix_world.translation = (0, 0, 0)

        # ~ hd_ob_copy_0 = utils.copy_object(ob_base_copy)
        # ~ mr = utils.create_multires_modifier(hd_ob_copy_0)
        # ~ utils.make_single_active(hd_ob_copy_0)
        # ~ for _ in range(0, self.hd_level):
            # ~ bpy.ops.object.multires_subdivide(modifier=mr.name, mode='CATMULL_CLARK')
        # ~ bpy.ops.object.multires_base_apply(modifier=mr.name)
        # ~ mr.levels = 0
        # ~ utils.apply_only_base_multiresolution_modifier(hd_ob_copy_0)
        # ~ initial_deltas = {}
        # ~ for i, v in enumerate(ob_base_copy.data.vertices):
            # ~ initial_deltas[i] = Vector( hd_ob_copy_0.data.vertices[i].co - v.co )
        # ~ utils.delete_object(hd_ob_copy_0)

        hd_ob_copy = utils.copy_object(self.hd_ob)
        utils.apply_only_base_multiresolution_modifier(hd_ob_copy)
        hd_ob_copy.modifiers.clear()
        utils.remove_shape_keys(hd_ob_copy)
        utils.delete_uv_layers(hd_ob_copy)
        hd_ob_copy.vertex_groups.clear()
        hd_ob_copy.data.materials.clear()
        hd_ob_copy.parent = None
        hd_ob_copy.matrix_world.translation = (0, 0, 0)

        assert( len(hd_ob_copy.data.vertices) == len(ob_base_copy.data.vertices) )
        delta_dz_min_len = 0.03
        deltas_values = []
        for i, v in enumerate(ob_base_copy.data.vertices):
            delta_blend = Vector( hd_ob_copy.data.vertices[i].co - v.co ) #- initial_deltas[i]
            delta_dz = (1.0 / self.gScale) * (self.blend_to_dz_mat @ delta_blend)
            if delta_dz.length > delta_dz_min_len:
                deltas_values.append( [i, delta_dz[0], delta_dz[1], delta_dz[2]] )

        utils.delete_object(hd_ob_copy)
        utils.delete_object(ob_base_copy)

        base_morph_info = {}
        base_morph_info["count"] = len(deltas_values)
        base_morph_info["values"] = deltas_values
        return base_morph_info

    def generate_dsf_file(self, context):
        dsf_new_id = self.morph_files_filename

        if not re.match(r"^\w+$", self.morph_files_filename):
            self.report({'ERROR'}, "Invalid .dsf filename given: \"{0}\".".format(self.morph_files_filename))
            return False

        base_morph_info = self.get_base_morph_info(context)
        if base_morph_info is None:
            return False

        self.dsf_fp = utils.copy_file( self.dsf_fp, self.morph_files_diroutput,
                                       self.morph_files_filename )

        dsf_j = utils.get_dsf_json(self.dsf_fp)
        try:
            dsf_orig_id = dsf_j["modifier_library"][0]["id"]
            dsf_morph = dsf_j["modifier_library"][0]["morph"]
            # ~ dsf_morph["vertex_count"] = 10
            dsf_morph["deltas"]["count"] = base_morph_info["count"]
            dsf_morph["deltas"]["values"] = base_morph_info["values"]
        except KeyError as e:
            self.report({'ERROR'}, ".dsf file given has invalid structure.")
            return False

        dsf_text = json.dumps(dsf_j)

        exp = "([^\w]){0}([^\w])".format(dsf_orig_id)
        exp = re.compile(exp)
        str_sub = "\g<1>{0}\g<2>".format(dsf_new_id)
        dsf_text = exp.sub(str_sub, dsf_text)

        utils.text_to_json_file(self.dsf_fp, dsf_text, with_gzip=True, indent=4)
        return True

    def generate_dhdm_file(self, context):
        print("Generating dhdm file...")
        filepaths_list = self.mfiles.get_filepaths(self.hd_level)

        f_name = "base"
        fp_base = self.export_ob_obj( self.base_ob, f_name, apply_modifiers=False )
        # ~ fp_base = self.export_ob_obj( self.hd_ob, f_name, apply_modifiers=False )

        hd_ob_ms = utils.ModifiersStatus(self.hd_ob, 'ENABLE_ONLY', m_types={'SUBDIV'})
        f_name += "_hd_edit"
        fp_hd_edit = self.export_ob_obj( self.hd_ob, f_name, apply_modifiers=True )
        hd_ob_ms.restore()
        del hd_ob_ms

        dll_wrapper.execute_in_new_thread( "generate_dhdm_file",
                                           self.gScale, fp_base, self.hd_level,
                                           self.morph_files_diroutput, self.morph_files_filename,
                                           filepaths_list )

        return True

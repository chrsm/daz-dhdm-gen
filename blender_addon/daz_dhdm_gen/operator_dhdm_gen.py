import os, time, json, re, bpy
from . import dll_wrapper
from . import utils
from .operator_common import dhdmGenBaseOperator
from mathutils import Vector, Matrix
from urllib.parse import quote


class GenerateNewMorphFiles(dhdmGenBaseOperator):
    """Generate .dsf and .dhdm files"""
    bl_idname = "dazdhdmgen.generatenewmorph"
    bl_label = "Generate .dsf/.dhdm files"

    hd_level = None
    subd_m = None
    morph_files_diroutput = None
    to_complete = None

    blend_to_dz_mat = Matrix([ (1, 0,  0),
                               (0, 0,  1),
                               (0, -1, 0) ])

    def execute(self, context):
        t0 = time.perf_counter()
        if not self.check_input(context, check_hd=True, check_morph_name=True):
            return {'CANCELLED'}
        if not self.get_hd_level():
            return {'CANCELLED'}
        if not self.check_all_matching_files(self.hd_level):
            return {'CANCELLED'}
        self.morph_files_diroutput = self.create_new_morphs_subdir()
        self.to_complete = False
        addon_props = context.scene.daz_dhdm_gen

        r = self.generate_dsf_file(context)
        if not r:
            self.cleanup(context)
            return {'CANCELLED'}

        r = self.generate_dhdm_file(context)
        self.cleanup(context)
        if not r:
            return {'CANCELLED'}

        if (addon_props.output_type == 'DHDM'):
            self.report({'INFO'}, ".dhdm file generated.")
        elif (addon_props.output_type == 'DSF_BASIC'):
            if self.to_complete:
                self.report({'INFO'}, ".dsf and .dhdm files generated, .dsf file must be completed (search \"[TO_COMPLETE]\" in it).")
            else:
                self.report({'INFO'}, ".dsf and .dhdm files generated.")
        else:
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
        if (subd_m is None) or (subd_m.type != 'MULTIRES'):
            self.report({'ERROR'}, "hd mesh has no multiresolution modifier.")
            return False
        if subd_level <= 0:
            self.report({'ERROR'}, "hd mesh is not subdivided.")
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
        ob_base_copy.vertex_groups.clear()
        ob_base_copy.data.materials.clear()
        utils.remove_shape_keys(ob_base_copy)
        utils.delete_uv_layers(ob_base_copy)
        ob_base_copy.parent = None
        ob_base_copy.matrix_world.translation = (0, 0, 0)

        ob_base_morphed_copy = None
        if self.morphed_base_ob is not None:
            ob_base_morphed_copy = utils.copy_object(self.morphed_base_ob)
            utils.apply_shape_keys(ob_base_morphed_copy)
        else:
            ob_base_morphed_copy = utils.copy_object(self.hd_ob)
            utils.remove_shape_keys(ob_base_morphed_copy)
        ob_base_morphed_copy.modifiers.clear()
        ob_base_morphed_copy.vertex_groups.clear()
        ob_base_morphed_copy.data.materials.clear()
        utils.delete_uv_layers(ob_base_morphed_copy)
        ob_base_morphed_copy.parent = None
        ob_base_morphed_copy.matrix_world.translation = (0, 0, 0)

        assert( len(ob_base_morphed_copy.data.vertices) == len(ob_base_copy.data.vertices) )
        delta_dz_min_len = 1e-3
        deltas_values = []
        for i, v in enumerate(ob_base_copy.data.vertices):
            delta_blend = Vector( ob_base_morphed_copy.data.vertices[i].co - v.co )
            delta_dz = (1.0 / self.gScale) * (self.blend_to_dz_mat @ delta_blend)
            if delta_dz.length > delta_dz_min_len:
                deltas_values.append( [i, delta_dz[0], delta_dz[1], delta_dz[2]] )
        utils.delete_object(ob_base_copy)
        utils.delete_object(ob_base_morphed_copy)

        base_morph_info = {}
        base_morph_info["count"] = len(deltas_values)
        base_morph_info["values"] = deltas_values
        return base_morph_info

    def generate_dsf_file(self, context):
        addon_props = context.scene.daz_dhdm_gen
        if (addon_props.output_type == 'DHDM'):
            return True
        dsf_new_id = self.morph_name

        if (addon_props.output_type == 'DSF_TEMPLATE'):
            dsf_fp_templ = os.path.abspath( bpy.path.abspath(addon_props.dsf_file_template) )
            if not utils.has_extension( dsf_fp_templ, "dsf" ):
                self.report({'ERROR'}, "Invalid Template .dsf file.")
                return False
            if not os.path.isfile(dsf_fp_templ):
                self.report({'ERROR'}, "Template .dsf file not found.")
                return False
            dsf_fp = utils.copy_file( dsf_fp_templ, self.morph_files_diroutput, dsf_new_id )
            del dsf_fp_templ
        else:   # addon_props.output_type == 'DSF_BASIC'
            dsf_fp_templ = os.path.join(os.path.dirname(__file__), "other_files", "dhdmGenHDMorph.dsf")
            if not os.path.isfile(dsf_fp_templ):
                self.report({'ERROR'}, "Basic template .dsf file not found.")
                return False
            dsf_fp = utils.copy_file( dsf_fp_templ, self.morph_files_diroutput, dsf_new_id )
            del dsf_fp_templ

        print("Generating dsf file...")
        base_morph_info = self.get_base_morph_info(context)
        dsf_j = utils.get_dsf_json(dsf_fp)
        try:
            dsf_orig_asset_id = os.path.basename(dsf_j["asset_info"]["id"]).rsplit(".", 1)[0]
            dsf_orig_id = dsf_j["modifier_library"][0]["id"]
            dsf_morph = dsf_j["modifier_library"][0]["morph"]

            if (addon_props.output_type == 'DSF_BASIC'):
                dsf_morph["vertex_count"] = len(self.base_ob.data.vertices)
                dz_dir = addon_props.morph_daz_directory.strip().replace("\\","/")
                if not dz_dir:
                    os.remove(dsf_fp)
                    self.report({'ERROR'}, "No morph daz directory given.")
                    return False
                if not dz_dir.startswith('/'):
                    os.remove(dsf_fp)
                    self.report({'ERROR'}, "Morph daz directory given is invalid (must start with '/').")
                    return False
                dz_dir = quote(dz_dir, safe="/#")
                dsf_j["asset_info"]["id"] = os.path.join(dz_dir, dsf_orig_id + ".dsf").replace("\\","/")
                dsf_morph["hd_url"] = dsf_j["asset_info"]["id"].rsplit(".", 1)[0] + ".dhdm"
                base_ob_url = getattr(self.base_ob, "DazUrl", "").strip()
                if len(base_ob_url) > 0:
                    base_ob_url = quote(base_ob_url.replace("\\","/"), safe="/#")
                    dsf_j["modifier_library"][0]["parent"] = base_ob_url
                else:
                    self.to_complete = True
            else:  # addon_props.output_type == 'DSF_TEMPLATE'
                vcount = dsf_morph["vertex_count"]
                if (vcount != -1) and (vcount != len(self.base_ob.data.vertices)):
                    os.remove(dsf_fp)
                    self.report({'ERROR'}, "Template .dsf file given is not valid for specified base mesh.")
                    return False
                dsf_morph["hd_url"] = dsf_j["asset_info"]["id"].rsplit(".", 1)[0] + ".dhdm"

            dsf_morph["deltas"]["count"] = base_morph_info["count"]
            dsf_morph["deltas"]["values"] = base_morph_info["values"]

        except KeyError as e:
            os.remove(dsf_fp)
            self.report({'ERROR'}, ".dsf file has invalid structure.")
            return False

        if ("scene" in dsf_j) and ("modifiers" in dsf_j["scene"]):
            dsf_modif = dsf_j["scene"]["modifiers"]
            n = 0
            for e in dsf_modif:
                if (e["id"] == dsf_orig_id) or (e["id"] == dsf_orig_asset_id):
                    e["id"] = "{}-{}".format(e["id"], n)
                    n += 1
            del n, dsf_modif

        if ("modifier_library" in dsf_j):
            dsf_modif_lib = dsf_j["modifier_library"]
            for e in dsf_modif_lib:
                if ("channel" not in e) or ("label" not in e["channel"]):
                    continue
                if (e["id"] == dsf_orig_id) or (e["id"] == dsf_orig_asset_id):
                    e["channel"]["label"] = dsf_new_id
            del dsf_modif_lib

        dsf_text = json.dumps(dsf_j)
        for dsf_id in [dsf_orig_id, dsf_orig_asset_id]:
            exp = "([^\w]){0}([^\w])".format(dsf_id)
            exp = re.compile(exp)
            str_sub = "\g<1>{0}\g<2>".format(dsf_new_id)
            dsf_text = exp.sub(str_sub, dsf_text)

        utils.text_to_json_file(dsf_fp, dsf_text, with_gzip=(not self.to_complete), indent=4)
        print("Finished generating .dsf file \"{0}\".".format(dsf_fp))
        return True

    def generate_dhdm_file(self, context):
        print("Generating dhdm file...")
        filepaths_list = self.mfiles.get_filepaths(self.hd_level, self.base_subdiv_method)

        base_ob_copy = None
        if self.morphed_base_ob is not None:
            base_ob_copy = utils.copy_object(self.morphed_base_ob)
            utils.apply_shape_keys(base_ob_copy)
        else:
            base_ob_copy = utils.copy_object(self.hd_ob)
            utils.remove_shape_keys(base_ob_copy)
        base_ob_copy.modifiers.clear()
        base_ob_copy.vertex_groups.clear()
        utils.delete_uv_layers(base_ob_copy)
        base_ob_copy.parent = None
        base_ob_copy.matrix_world.translation = (0, 0, 0)

        f_name_base = "base"
        fp_base = self.export_ob_obj( base_ob_copy, f_name_base, apply_modifiers=False )

        f_name = f_name_base + "_hd_no_edit"
        if self.base_subdiv_method == 'MULTIRES':
            base_ob_copy.data.materials.clear()
            utils.subdivide_object_m(base_ob_copy, self.subd_m, self.hd_level)
            fp_hd_no_edit = self.export_ob_obj( base_ob_copy, f_name, apply_modifiers=True )
            utils.delete_object(base_ob_copy)
        else: # MULTIRES_REC
            outputDirpath = self.create_temporary_subdir()
            hd_base = utils.api_generate_simple_hd_mesh(context, base_ob_copy, self.hd_level, self.gScale, outputDirpath)
            utils.delete_object(base_ob_copy)
            if hd_base is None:
                print("Required addon \"daz_hd_morphs\" not found or function api_generate_simple_hd_mesh() failed.")
                self.report({'ERROR'}, "Operator failed (see console output).")
                return False
            utils.create_unsubdivide_multires(hd_base)
            fp_hd_no_edit = self.export_ob_obj( hd_base, f_name, apply_modifiers=True )
            utils.delete_object(hd_base)
            del hd_base, outputDirpath
        del base_ob_copy

        hd_ob_ms = utils.ModifiersStatus(self.hd_ob, 'ENABLE_ONLY', m_types={'SUBDIV'})
        f_name = f_name_base + "_hd_edit"
        fp_hd_edit = self.export_ob_obj( self.hd_ob, f_name, apply_modifiers=True )
        hd_ob_ms.restore()
        del hd_ob_ms

        dll_wrapper.execute_in_new_thread( "generate_dhdm_file",
                                           self.gScale, fp_base, self.hd_level,
                                           self.morph_files_diroutput, self.morph_name,
                                           filepaths_list )

        fp_dhdm = os.path.join(self.morph_files_diroutput, self.morph_name + ".dhdm")
        print("Finished generating .dhdm file \"{0}\".".format(fp_dhdm))
        return True

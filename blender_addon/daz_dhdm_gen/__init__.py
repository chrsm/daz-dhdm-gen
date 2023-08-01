import bpy
from . import operator_dhdm_gen
from . import operator_match_gen


bl_info = {
    'name': 'daz dhdm generator',
    'author': 'Xin',
    'version': (0, 0, 6),
    'blender': (3, 6, 0),
    'location': '3d view > N panel > dhdm tab',
    'description': 'Generate .dhdm files from Blender meshes',
    "doc_url": "https://bitbucket.org/Diffeomorphic/import_daz/issues/1399/blender-addon-to-generate-rigged-hd-meshes",
    "tracker_url": "https://gitlab.com/x190/daz-dhdm-gen",
    'category': 'Mesh'
}

class dhdmGenProperties(bpy.types.PropertyGroup):
    working_dirpath:    bpy.props.StringProperty( subtype="DIR_PATH", name="Working directory" )

    unit_scale:         bpy.props.FloatProperty( name="Unit scale", default=0.01, min=0.00001,
                                description= "Scale used to convert from daz's units to Blender's units" )

    base_ob:            bpy.props.StringProperty( name="Base mesh", description= "Base mesh" )

    matching_files_dir: bpy.props.StringProperty(subtype="DIR_PATH", name="Matching files directory")

    hd_ob:              bpy.props.StringProperty( name="HD mesh", description= "HD mesh" )

    base_subdiv_method: bpy.props.EnumProperty( name = "Subdiv method",
                                                items = ( ('MULTIRES', "From direct multires", "hd mesh has a (non-applied) multiresolution modifier"),
                                                          ('MULTIRES_REC', "From daz_hd_morphs multires", "hd mesh was generated with daz_hd_morphs addon"), ),
                                                default = 'MULTIRES',
                                                description = "Subdivision method used to generate the hd mesh"
                                               )

    base_morphs: bpy.props.EnumProperty( name = "Base morphs",
                                         items = ( ('HD_MESH', "From hd mesh", "Use base level of hd mesh as base morph data"),
                                                   ('BASE_MORPHED', "From morphed base mesh", "Get base morph data from morphed base mesh"), ),
                                         default = 'HD_MESH',
                                         description = "Base morph data used by the new morph"
                                       )

    morphed_base_ob:  bpy.props.StringProperty( name="Morphed base mesh", description="Base mesh morphed with the base morphs used to derive the hd mesh" )

    morph_name:  bpy.props.StringProperty(name="New morph name", description="New morph name (without extension)", default="")

    only_dhdm:  bpy.props.BoolProperty(name="Only .dhdm", default=False, description="Generate .dhdm file only (without .dsf file)")

    dsf_file_template:  bpy.props.StringProperty(subtype="FILE_PATH", name="Template .dsf file")


class AddonPanel:
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "dhdm"

    @classmethod
    def poll(cls, context):
        return context.mode == 'OBJECT'


class PANEL_PT_dhdmGenhPanel(AddonPanel, bpy.types.Panel):
    bl_label = "Main settings"
    bl_idname = "PANEL_PT_dhdmGenhPanel"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        addon_props = context.scene.daz_dhdm_gen

        box = layout.box()
        row = box.row()
        row.prop(addon_props, "working_dirpath")
        row = box.row()
        row.prop(addon_props, "unit_scale")
        row = box.row()
        row.prop_search(addon_props, "base_ob", context.scene, "objects")
        row = box.row()
        row.prop(addon_props, "matching_files_dir")
        row = box.row()
        row.operator(operator_match_gen.GenerateMatching.bl_idname)


class PANEL_PT_dhdmGenHDPanel(AddonPanel, bpy.types.Panel):
    bl_idname = "PANEL_PT_dhdmGenHDPanel"
    bl_label = "New HD morph"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        addon_props = context.scene.daz_dhdm_gen

        row = layout.row()
        row.prop_search(addon_props, "hd_ob", context.scene, "objects")
        row = layout.row()
        row.prop(addon_props, "base_subdiv_method")
        row = layout.row()
        row.prop(addon_props, "base_morphs")
        if addon_props.base_morphs == 'BASE_MORPHED':
            row = layout.row()
            row.prop_search(addon_props, "morphed_base_ob", context.scene, "objects")
        row = layout.row()
        row.prop(addon_props, "morph_name")
        row = layout.row()
        row.prop(addon_props, "only_dhdm")
        if not addon_props.only_dhdm:
            row = layout.row()
            row.prop(addon_props, "dsf_file_template")
        row = layout.row()
        row.operator(operator_dhdm_gen.GenerateNewMorphFiles.bl_idname)


class dazHDCustomPreferences(bpy.types.AddonPreferences):
    bl_idname = __name__

    delete_temporary_files:  bpy.props.BoolProperty(
                                name="Delete temporary files", default=True,
                                description="Delete .obj/.mtl/.dae files in the working directory after operators finish"
                             )

    def draw(self, context):
        box = self.layout.box()
        row = box.row()
        row.prop(self, "delete_temporary_files")


classes = (
    dhdmGenProperties,
    dazHDCustomPreferences,

    operator_dhdm_gen.GenerateNewMorphFiles,
    operator_match_gen.GenerateMatching,

    PANEL_PT_dhdmGenhPanel,
    PANEL_PT_dhdmGenHDPanel,
)

def register():
    for c in classes:
        bpy.utils.register_class(c)
    bpy.types.Scene.daz_dhdm_gen = bpy.props.PointerProperty(type=dhdmGenProperties)

def unregister():
    del bpy.types.Scene.daz_dhdm_gen
    for c in reversed(classes):
        bpy.utils.unregister_class(c)

import bpy
from . import operator_dhdm_gen
from . import operator_match_gen


bl_info = {
    'name': 'daz dhdm generator',
    'author': 'Xin',
    'version': (0, 0, 2),
    'blender': (3, 6, 0),
    'location': '3d view > N panel > dhdm tab',
    'description': 'Generate .dhdm files from Blender meshes',
    "doc_url": "https://bitbucket.org/Diffeomorphic/import_daz/issues/1399/blender-addon-to-generate-rigged-hd-meshes",
    "tracker_url": "https://gitlab.com/x190/daz-hd-morphs",
    'category': 'Mesh'
}

class dhdmGenProperties(bpy.types.PropertyGroup):
    working_dirpath:    bpy.props.StringProperty( subtype="DIR_PATH", name="Working directory" )

    unit_scale:         bpy.props.FloatProperty( name="Unit scale", default=0.01, min=0.00001,
                                description= "Scale used to convert from daz's units to Blender's units" )

    base_ob:            bpy.props.StringProperty( name="Base mesh", description= "Base mesh" )

    matching_files_dir: bpy.props.StringProperty(subtype="DIR_PATH", name="Matching files directory")

    hd_ob:              bpy.props.StringProperty( name="HD mesh", description= "HD mesh" )

    only_dhdm:          bpy.props.BoolProperty(name="Only .dhdm", default=False, description="Generate .dhdm file only")

    dsf_file_template:  bpy.props.StringProperty(subtype="FILE_PATH", name="Template .dsf file")

    morph_name:  bpy.props.StringProperty(name="New morph name", description="New morph name (without extension)", default="")

    base_subdiv_method: bpy.props.EnumProperty( name = "Subdiv method",
                                                items = ( ('SUBSURF', "Applied subsurf (limit surface off)", "Blender subsurface modifier (limit surface off) has been applied to the hd mesh"),
                                                          ('SUBSURF_LIMIT', "Applied subsurf (limit surface on)", "Blender subsurface modifier (limit suface on) has been applied to the hd mesh"),
                                                          ('MULTIRES', "Applied multires", "Blender multiresolution modifier has been applied has been applied to the hd mesh"),
                                                          ('DAZ', "daz", "Applied daz subdivision"),
                                                          ('MODIFIER', "From modifier", "hd mesh has a (non-applied) subdivision modifier"), ),
                                                default = 'MODIFIER',
                                                description = "Subdivision method you used to obtain the hd mesh from the base mesh"
                                                )

    apply_dsf_base_morph: bpy.props.BoolProperty(name="With .dsf base morph", default=False, description="hd mesh had .dsf file base morph applied")


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

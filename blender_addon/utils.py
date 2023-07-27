import bpy, os, math, time, gzip, json, re, shutil
from urllib.parse import unquote
from mathutils import Vector


def has_extension(filepath, *exts):
    assert(len(exts) > 0)
    t = os.path.basename(filepath).rsplit(".", 1)
    return (len(t) == 2 and t[1].lower() in exts)

def get_extension(filepath):
    t = os.path.basename(filepath).rsplit(".", 1)
    if len(t) != 2:
        return None
    return t[1].lower()

def remove_extension(filename):
    t = filename.rsplit(".", 1)
    return t[0].strip()

def makeValidFilename(name):
    name = name.strip().lower()
    return "".join( c for c in name if (c.isalnum() or c in "._- ") )

def delete_object(ob):
    assert(ob.type == 'MESH')
    ob_data = ob.data
    ob_mats = set(ob_data.materials)
    bpy.data.objects.remove(ob, do_unlink=True, do_id_user=True, do_ui_user=True)
    if ob_data is not None and ob_data.users == 0:
        bpy.data.meshes.remove(ob_data, do_unlink=True, do_id_user=True, do_ui_user=True)
    for m in ob_mats:
        if m is not None and m.users == 0:
            bpy.data.materials.remove(m, do_unlink=True, do_id_user=True, do_ui_user=True)

def delete_armature(ob):
    assert(ob.type == 'ARMATURE')
    ob_data = ob.data
    bpy.data.objects.remove(ob, do_unlink=True, do_id_user=True, do_ui_user=True)
    if ob_data is not None and ob_data.users == 0:
        bpy.data.armatures.remove(ob_data, do_unlink=True, do_id_user=True, do_ui_user=True)

def delete_object_materials(ob):
    mats = set(ob.data.materials)
    ob.data.materials.clear()
    for m in mats:
        if m is not None and m.users == 0:
            bpy.data.materials.remove(m, do_unlink=True, do_id_user=True, do_ui_user=True)

def make_single_active(ob):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob

def make_final_active(ob):
    arm = get_armature(ob)
    if arm:
        make_single_active(arm)
    else:
        make_single_active(ob)

def import_dll_obj(fp):
    if not has_extension(fp, "obj"):
        raise ValueError("File \"{0}\" is not a .obj file.".format(fp))
    if not os.path.isfile(fp):
        raise ValueError("File \"{0}\" not found.".format(fp))
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.wm.obj_import( filepath=fp, check_existing=False,
                           import_vertex_groups=False, validate_meshes=False, )
                           # forward_axis='Y', up_axis='Z' )
    hd_ob = bpy.context.selected_objects[0]
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    bpy.ops.object.shade_smooth()
    return hd_ob

def import_dll_collada(fp):
    if not has_extension(fp, "dae"):
        raise ValueError("File \"{0}\" is not a .dae file.".format(fp))
    if not os.path.isfile(fp):
        raise ValueError("File \"{0}\" not found.".format(fp))
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.wm.collada_import( filepath=fp, import_units=False, custom_normals=False,
                               fix_orientation=False, find_chains=False, auto_connect=False,
                               min_chain_length=0, keep_bind_info=False )
    hd_ob = None
    for ob in bpy.context.selected_objects:
        if ob.type == 'ARMATURE':
            delete_armature(ob)
        else:
            remove_armature_modifiers(ob)
            hd_ob = ob
    bpy.ops.object.shade_smooth()
    return hd_ob

def add_collection(c_name):
    if c_name not in bpy.data.collections:
        bpy.data.collections.new(c_name)
    c = bpy.data.collections[c_name]
    if c.name not in bpy.context.scene.collection.children:
        bpy.context.scene.collection.children.link(c)
    return c

def move_to_collection(ob, c_names):
    c_dests = []
    if c_names is not None and len(c_names) > 0 and c_names[0] is not None:
        for c_name in c_names:
            c = add_collection(c_name)
            c_dests.append(c)
    else:
        c_dests.append( bpy.context.window.view_layer.layer_collection.collection )
    for c in ob.users_collection:
        c.objects.unlink(ob)
    for c in c_dests:
        c.objects.link(ob)

def copy_object(ob, collection_name=None):
    ob2 = ob.copy()
    ob2.data = ob.data.copy()
    move_to_collection(ob2, [collection_name])
    return ob2

def get_armature(ob):
    for m in ob.modifiers:
        if m.type == 'ARMATURE':
            return m.object
    return None

def parent_to(ob_h, ob_p):
    mat_world = ob_h.matrix_world.copy()
    ob_h.parent = ob_p
    ob_h.matrix_world = mat_world

def create_unsubdivide_multires(ob):
    if len(ob.modifiers) != 0 or ob.data.shape_keys is not None:
            raise RuntimeError("Object has modifiers or shape keys")
        # ~ ob.modifiers.clear()
        # ~ remove_shape_keys(ob)

    mr = ob.modifiers.new("daz_dhdm_gen_multires", 'MULTIRES')

    mr.show_only_control_edges = True
    mr.quality = 4
    mr.uv_smooth = 'PRESERVE_CORNERS'
    mr.boundary_smooth = 'ALL'
    mr.use_creases = True
    mr.use_custom_normals = False

    make_single_active(ob)
    bpy.ops.object.multires_rebuild_subdiv(modifier=mr.name)
    # ~ bpy.ops.object.multires_unsubdivide(modifier=mr.name)
    # ~ mr.levels = 0
    bpy.ops.object.select_all(action='DESELECT')

def create_subsurf_modifier(ob, use_limit=False):
    sd = ob.modifiers.new("daz_dhdm_gen_subsurf", 'SUBSURF')
    sd.show_only_control_edges = True
    sd.use_limit_surface = use_limit
    sd.uv_smooth = 'PRESERVE_CORNERS'
    sd.boundary_smooth = 'ALL'
    sd.use_creases = True
    sd.use_custom_normals = False
    return sd

def create_multires_modifier(ob):
    mr = ob.modifiers.new("daz_dhdm_gen_multires", 'MULTIRES')
    mr.show_only_control_edges = True
    mr.quality = 4
    mr.uv_smooth = 'PRESERVE_CORNERS'
    mr.boundary_smooth = 'ALL'
    mr.use_creases = True
    mr.use_custom_normals = False
    return mr

def get_subdivision_level(base_ob, hd_ob):
    # ~ nt = 0
    # ~ nq = 0
    # ~ for p in base_ob.data.polygons:
        # ~ nv = len(p.vertices)
        # ~ if nv == 4:
            # ~ nq += 1
        # ~ elif nv == 3:
            # ~ nt += 1

    # ~ nq2 = len(hd_ob.data.polygons)
    # ~ level = int( math.log(nq2/(4*nq + 3*nt)) / math.log(4) ) + 1

    # this only works right for mixtures of triangles and quads (no n-gons):
    nq = len(base_ob.data.polygons)
    nq2 = len(hd_ob.data.polygons)
    level = round( math.log(nq2/nq) / math.log(4) )

    return level

def remove_armature_modifiers(ob):
    for m in ob.modifiers:
        if m.type == 'ARMATURE':
            ob.modifiers.remove(m)

def remove_division_modifiers(ob):
    for m in ob.modifiers:
        if m.type in ('SUBSURF', 'MULTIRES'):
            ob.modifiers.remove(m)

def remove_shape_keys(ob):
    if ob.data.shape_keys is not None and len(ob.data.shape_keys.key_blocks) > 0:
        ob.active_shape_key_index = 0
        ob.shape_key_clear()

def read_dhdm_level(dhdm_fp):
    if not os.path.isfile(dhdm_fp):
        raise ValueError("File \"{}\" not found.".format(dhdm_fp))
    with open(dhdm_fp, "rb") as f:
        h = f.read(8)
    return int.from_bytes(h[4:], byteorder='little', signed=False)

def is_gzip_file(fp):
    with open(fp, "rb") as f:
        return f.read(2) == b'\x1f\x8b'

def get_dsf_json(dsf_fp):
    j = None
    if is_gzip_file(dsf_fp):
        with gzip.open(dsf_fp, "rt", encoding="utf-8") as f:
            j = json.loads(f.read())
    else:
        with open(dsf_fp, "r", encoding="utf-8") as f:
            j = json.loads(f.read())
    return j

def read_dsf_level(dsf_fp):
    j = get_dsf_json(dsf_fp)
    try:
        dhdm = j["modifier_library"][0]["morph"]["hd_url"]
        dhdm_fp = os.path.join( os.path.dirname(dsf_fp), os.path.basename(dhdm) )
        dhdm_fp = unquote(dhdm_fp)
        return read_dhdm_level(dhdm_fp)
    except KeyError:
        return 0

def read_dsf_id(dsf_fp, only_with_dhdm=False):
    j = get_dsf_json(dsf_fp)
    try:
        if only_with_dhdm:
            dhdm = j["modifier_library"][0]["morph"]["hd_url"]
        return j["modifier_library"][0]["id"]
    except KeyError:
        return None

def dsf_vcount(dsf_fp):
    j = get_dsf_json(dsf_fp)
    try:
        v_count = j["modifier_library"][0]["morph"]["vertex_count"]
        return v_count
    except KeyError:
        return None

def get_file_id(filepath):
    filename = os.path.basename(filepath).rsplit(".",1)[0]

    if has_extension(filepath, "dhdm"):
        dsf_fp = os.path.join(os.path.dirname(filepath), filename + ".dsf")
        if not os.path.isfile(dsf_fp):
            return filename
        filepath = dsf_fp
    elif not has_extension(filepath, "dsf"):
        raise ValueError("File \"{0}\" is not .dsf or .dhdm file.".format(filepath))

    dsf_id = read_dsf_id(filepath)
    if dsf_id is not None:
        return dsf_id
    return filename

def get_selected_meshes(context):
    return [ ob for ob in context.view_layer.objects
             if ob.select_get() and ob.type == 'MESH'
                and not (ob.hide_get() or ob.hide_viewport) ]

def delete_uv_layers(ob, preserve=[]):
    preserve = set(preserve)
    to_remove = []
    for i, layer in enumerate(ob.data.uv_layers):
        if i not in preserve:
            to_remove.append(layer)
    for layer in to_remove:
        ob.data.uv_layers.remove(layer)

def check_dsf_file(fp, base_vcount, geo_vcounts, with_print=True):
    fp = fp.strip()
    if not fp:
        return None
    if not has_extension(fp, "dsf"):
        if with_print:
            print("  File \"{0}\" has no .dsf extension.".format(fp))
        return None
    fp = os.path.abspath( bpy.path.abspath( fp ) )
    if not os.path.isfile(fp):
        if with_print:
            print("  File \"{0}\" not found.".format(fp))
        return None

    v_count = dsf_vcount(fp)
    if v_count is None:
        if with_print:
            print("  File \"{0}\" describes no geometry.".format(fp))
        return None
    if v_count >= 0:
        if geo_vcounts is None:
            if v_count != base_vcount:
                if with_print:
                    print("  File \"{0}\" vertex count mismatch with base mesh.".format(fp))
                return None
        elif v_count not in geo_vcounts:
            if with_print:
                print("  File \"{0}\" vertex count mismatch with all geografts.".format(fp))
            return None
    return fp


class ModifiersStatus:
    def __init__(self, ob, mode, m_types):
        assert( mode in {'ENABLE', 'DISABLE', 'ENABLE_ONLY', 'DISABLE_ONLY'} )

        new_status = mode.startswith("ENABLE")
        change_rest_opposite = mode.endswith("_ONLY")

        change_all = False
        types_to_change = set()
        for t in m_types:
            if t ==  'ALL':
                if len(m_types) > 1:
                    raise ValueError("Invalid \"m_types\" parameter: \"{0}\".".format(m_types))
                change_all = True
                break
            elif t == 'ARMATURE':
                types_to_change.add('ARMATURE')
            elif t == 'SUBDIV':
                types_to_change.update(('SUBSURF', 'MULTIRES'))
            else:
                raise ValueError("Invalid \"m_types\" parameter: \"{0}\".".format(m_types))

        self.ob = ob
        self.m_n = len(self.ob.modifiers)
        self.m_status = {}
        for i, m in enumerate(self.ob.modifiers):
            if change_all:
                self.m_status[i] = m.show_viewport
                m.show_viewport = new_status
            else:
                if m.type in types_to_change:
                    self.m_status[i] = m.show_viewport
                    m.show_viewport = new_status
                elif change_rest_opposite:
                    self.m_status[i] = m.show_viewport
                    m.show_viewport = not new_status

    def restore(self):
        assert( len(self.ob.modifiers) == self.m_n )
        for i, m in enumerate(self.ob.modifiers):
            if i in self.m_status:
                m.show_viewport = self.m_status[i]
        self.ob = None
        self.m_n = None
        self.m_status = None

def apply_shape_keys(ob):
    if ob.data.shape_keys is not None:
        make_single_active(ob)
        bpy.ops.object.shape_key_remove(all=True, apply_mix=True)

def apply_modifiers_stack(ob):
    make_single_active(ob)
    for m in ob.modifiers:
        bpy.ops.object.modifier_apply(modifier=m.name)

def ob_apply_modifiers(ob, modifiers='ALL'):
    assert( ob.type == 'MESH' )
    assert( modifiers in {'SK', 'NO_SK', 'NO_ARMATURE', 'ALL'} )
    if modifiers == 'SK':
        ob.modifiers.clear()
    elif modifiers == 'NO_ARMATURE':
        remove_armature_modifiers(ob)
    elif modifiers == 'NO_SK':
        remove_shape_keys(ob)
    apply_shape_keys(ob)
    apply_modifiers_stack(ob)

def get_fingerprint(ob):
    if ob.type != 'MESH':
        raise ValueError("Object \"{0}\" is not a mesh.".format(ob.name))
    return "{0}-{1}-{2}".format( len(ob.data.vertices), len(ob.data.edges), len(ob.data.polygons) )

def get_info_from_filename(filename):
    r = re.search(r'^f(\d+-\d+-\d+)_div(\d)_(mr|mrr)\.json$', filename)
    if r is None:
        return None, None, None
    return r.group(1), int(r.group(2)), r.group(3)

def copy_file(fp, target_dir, target_filename=None):
    if not os.path.isdir(target_dir):
        raise ValueError("Directory \"{0}\" not found.".format(target_dir))
    if not os.path.isfile(fp):
        raise ValueError("File \"{0}\" not found.".format(fp))

    if target_filename is None:
        target_fp = os.path.join(target_dir, os.path.basename(fp))
    else:
        ext = get_extension(fp)
        target_fp = os.path.join(target_dir, "{0}.{1}".format(target_filename, ext))

    new_fp = shutil.copyfile( fp, target_fp )
    return new_fp

def text_to_json_file(fp, text, with_gzip=True, indent=None):
    j = json.loads(text)
    j_to_json_file(fp, j, with_gzip=with_gzip, indent=indent)

def j_to_json_file(fp, j, with_gzip=True, indent=None):
    if with_gzip:
        with gzip.open(fp, 'wt', encoding="utf-8") as f:
            json.dump(j, f, indent=indent)
    else:
        with open(fp, 'w', encoding="utf-8") as f:
            json.dump(j, f, indent=indent)

def apply_only_base_multiresolution_modifier(ob):
    for m in ob.modifiers:
        if m.type == 'MULTIRES':
            make_single_active(ob)
            m.levels = 0
            bpy.ops.object.modifier_apply(modifier=m.name)
            break
        else:
            ob.modifiers.remove(m)

def subdivide_object_m(ob, m, levels):
    assert(m.type in ('SUBSURF', 'MULTIRES'))

    if m.type == 'SUBSURF':
        sd = ob.modifiers.new("daz_dhdm_gen_subsurf", 'SUBSURF')
        sd.show_only_control_edges = True
        sd.use_limit_surface = m.use_limit_surface
        sd.quality = m.quality
        sd.uv_smooth = m.uv_smooth
        sd.boundary_smooth = m.boundary_smooth
        sd.use_creases = m.use_creases
        sd.use_custom_normals = m.use_custom_normals
        sd.levels = levels
    else:
        mr = ob.modifiers.new("daz_dhdm_gen_multires", 'MULTIRES')
        mr.show_only_control_edges = True
        mr.quality = m.quality
        mr.uv_smooth = m.uv_smooth
        mr.boundary_smooth = m.boundary_smooth
        mr.use_creases = m.use_creases
        mr.use_custom_normals = m.use_custom_normals
        make_single_active(ob)
        for _ in range(0, levels):
            bpy.ops.object.multires_subdivide(modifier=mr.name, mode='CATMULL_CLARK')

def subdivide_object(ob, method, levels):
    assert(method in ('SUBSURF', 'SUBSURF_LIMIT', 'MULTIRES'))
    if method in ('SUBSURF', 'SUBSURF_LIMIT'):
        sd = ob.modifiers.new("daz_dhdm_gen_subsurf", 'SUBSURF')
        sd.show_only_control_edges = True
        sd.use_limit_surface = (method == 'SUBSURF_LIMIT')
        sd.levels = levels
    else:
        mr = ob.modifiers.new("daz_dhdm_gen_multires", 'MULTIRES')
        mr.show_only_control_edges = True
        make_single_active(ob)
        for _ in range(0, levels):
            bpy.ops.object.multires_subdivide(modifier=mr.name, mode='CATMULL_CLARK')

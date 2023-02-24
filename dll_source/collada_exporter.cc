#include <iostream>
#include <sstream>
#include <fmt/format.h>
#include <algorithm>

#include "mesh.hh"


using namespace tinyxml2;

std::string vgroup_name_sid(std::string vgname)
{
    std::replace(vgname.begin(), vgname.end(), ' ', '_');
    return vgname;
}

XMLElement* makeParam(XMLDocument & dae, const char* name, const char* type)
{
    XMLElement* param = dae.NewElement("param");
    param->SetAttribute("name", name);
    param->SetAttribute("type", type);
    return param;
}

XMLElement* makeAccesor(XMLDocument & dae, const char* source, const size_t count_, const char* stride)
{
    XMLElement* accesor = dae.NewElement("accessor");
    accesor->SetAttribute("source", source);
    accesor->SetAttribute("count", count_);
    accesor->SetAttribute("stride", stride);
    return accesor;
}

XMLElement* makeInput(XMLDocument & dae, const char* semantic, const char* source)
{
    XMLElement* input = dae.NewElement("input");
    input->SetAttribute("semantic", semantic);
    input->SetAttribute("source", source);
    return input;
}

XMLElement* makeInput(XMLDocument & dae, const char* semantic, const char* source, const char* offset)
{
    XMLElement* input = makeInput(dae, semantic, source);
    input->SetAttribute("offset", offset);
    return input;
}


void dhdm::Mesh::insertVerticesPosSource(XMLDocument & dae, XMLElement* parent) {
    XMLElement* source = dae.NewElement("source");
    source->SetAttribute("id", "mesh_positions");
    parent->InsertEndChild(source);

    XMLElement* float_array = dae.NewElement("float_array");
    float_array->SetAttribute("id", "mesh_positions_array");
    float_array->SetAttribute("count", vertices.size() * 3);
    std::ostringstream ss(std::ostringstream::out);
    for (auto & v : vertices) {
        const glm::dvec3 pos = v.pos * dhdm::gScale;
        ss << fmt::format("{} {} {} ", (float) pos.x, (float) pos.y, (float) pos.z);
    }
    float_array->SetText(ss.str().c_str());
    source->InsertEndChild(float_array);

    XMLElement* common = dae.NewElement("technique_common");
    source->InsertEndChild(common);

    XMLElement* accesor = makeAccesor(dae, "#mesh_positions_array", vertices.size(), "3");
    common->InsertEndChild(accesor);

    accesor->InsertEndChild( makeParam(dae, "X", "float") );
    accesor->InsertEndChild( makeParam(dae, "Y", "float") );
    accesor->InsertEndChild( makeParam(dae, "Z", "float") );
}

void dhdm::Mesh::insertUVsSource( XMLDocument & dae, XMLElement* parent,
                                  const std::vector<std::string> & uv_layers_names )
{
    if (!uses_uvs) return;
    for (size_t i = 0; i < uv_layers.size(); i++)
    {
        XMLElement* source = dae.NewElement("source");
        source->SetAttribute("id", uv_layers_names[i].c_str());
        parent->InsertEndChild(source);

        XMLElement* float_array = dae.NewElement("float_array");
        float_array->SetAttribute("id", fmt::format("{}_array", uv_layers_names[i]).c_str());
        float_array->SetAttribute("count", uv_layers[i].size() * 2);
        std::ostringstream ss(std::ostringstream::out);
        for (auto & uv : uv_layers[i])
            ss << fmt::format("{} {} ", (float) uv.pos.x, (float) uv.pos.y);
        float_array->SetText(ss.str().c_str());
        source->InsertEndChild(float_array);

        XMLElement* common = dae.NewElement("technique_common");
        source->InsertEndChild(common);

        XMLElement* accesor = makeAccesor( dae,
                                           fmt::format("#{}_array", uv_layers_names[i]).c_str(),
                                           uv_layers[i].size(),
                                           "2");
        common->InsertEndChild(accesor);

        accesor->InsertEndChild( makeParam(dae, "S", "float") );
        accesor->InsertEndChild( makeParam(dae, "T", "float") );
    }
}


void insertMaterialPolylist( XMLDocument & dae, XMLElement* parent,
                             const std::vector<dhdm::Face> & mat_faces,
                             const short mat_slot,
                             const std::vector<std::string> & uv_layers_names )
{
    XMLElement* polylist = dae.NewElement("polylist");
    polylist->SetAttribute("count", mat_faces.size());
    if (mat_slot >= 0)
        polylist->SetAttribute("material", fmt::format("mat{}", mat_slot).c_str());
    parent->InsertEndChild(polylist);

    polylist->InsertEndChild( makeInput(dae, "VERTEX", "#mesh_vertices", "0") );
    for (size_t i = 0; i < uv_layers_names.size(); i++) {
        XMLElement* input = makeInput( dae,
                                       "TEXCOORD",
                                       fmt::format("#{}", uv_layers_names[i]).c_str(),
                                       "1");
        input->SetAttribute("set", i);
        polylist->InsertEndChild(input);
    }

    std::ostringstream ss(std::ostringstream::out);
    for (size_t i = 0; i < mat_faces.size(); i++)
        ss << "4 ";
    XMLElement* vcount = dae.NewElement("vcount");
    vcount->SetText(ss.str().c_str());
    polylist->InsertEndChild(vcount);

    ss.str("");

    // uses_uvs
    if ( uv_layers_names.size() > 0 )
    {
        for (auto & face : mat_faces) {
            for (auto & fv : face.vertices)
                ss << fmt::format("{} {} ", fv.vertex, fv.uv);
        }
    }
    else
    {
        for (auto & face : mat_faces) {
            for (auto & fv : face.vertices)
                ss << fmt::format("{} ", fv.vertex);
        }
    }
    XMLElement* p = dae.NewElement("p");
    p->SetText(ss.str().c_str());
    polylist->InsertEndChild(p);
}


void dhdm::Mesh::insertPolylist( XMLDocument & dae, XMLElement* parent,
                                 const std::vector< std::vector<Face> > & materials_faces,
                                 const std::vector<std::string> & uv_layers_names )
{
    XMLElement* vertices = dae.NewElement("vertices");
    vertices->SetAttribute("id", "mesh_vertices");
    parent->InsertEndChild(vertices);
    vertices->InsertEndChild( makeInput(dae, "POSITION", "#mesh_positions") );

    if (uses_materials) {
        for (size_t i=0; i < materials_faces.size(); i++)
            insertMaterialPolylist(dae, parent, materials_faces[i], (short) i, uv_layers_names);
    }
    else {
        insertMaterialPolylist(dae, parent, faces, -1, uv_layers_names);
    }
}

void dhdm::Mesh::insertJointsSource(XMLDocument & dae, XMLElement* parent) {
    XMLElement* source = dae.NewElement("source");
    source->SetAttribute("id", "joints");
    parent->InsertEndChild(source);

    XMLElement* name_array = dae.NewElement("Name_array");
    name_array->SetAttribute("id", "joints_array");
    name_array->SetAttribute("count", vgroupsNames.size());

    std::ostringstream ss(std::ostringstream::out);
    for (auto & vgn : vgroupsNames)
        ss << vgroup_name_sid(vgn) + " ";
    name_array->SetText(ss.str().c_str());
    source->InsertEndChild(name_array);

    XMLElement* common = dae.NewElement("technique_common");
    source->InsertEndChild(common);

    XMLElement* accesor = makeAccesor(dae, "#joints_array", vgroupsNames.size(), "1");
    common->InsertEndChild(accesor);

    accesor->InsertEndChild( makeParam(dae, "JOINT", "name") );
}

void dhdm::Mesh::insertBindPosesSource(XMLDocument & dae, XMLElement* parent) {
    XMLElement* source = dae.NewElement("source");
    source->SetAttribute("id", "bind_poses");
    parent->InsertEndChild(source);

    const size_t n = vgroupsNames.size()*16;
    XMLElement* float_array = dae.NewElement("float_array");
    float_array->SetAttribute("id", "bind_poses_array");
    float_array->SetAttribute("count", n);

    std::ostringstream ss(std::ostringstream::out);
    for (size_t i=0; i < n; i++)
        ss << "0 ";
    float_array->SetText(ss.str().c_str());
    source->InsertEndChild(float_array);

    XMLElement* common = dae.NewElement("technique_common");
    source->InsertEndChild(common);

    XMLElement* accesor = makeAccesor(dae, "#bind_poses_array", 0, "16");
    common->InsertEndChild(accesor);
}

void dhdm::Mesh::insertWeightsSource(XMLDocument & dae, XMLElement* parent) {
    XMLElement* source = dae.NewElement("source");
    source->SetAttribute("id", "weights");
    parent->InsertEndChild(source);

    XMLElement* float_array = dae.NewElement("float_array");
    float_array->SetAttribute("id", "weights_array");


    std::ostringstream ss(std::ostringstream::out);
    size_t weights_count = 0;
    for (auto & vw : vweights) {
        for (auto it = vw.weights.begin(); it != vw.weights.end(); it++) {
            ss << fmt::format("{} ", it->second);
        }
        weights_count += vw.weights.size();
    }
    float_array->SetAttribute("count", weights_count);
    float_array->SetText(ss.str().c_str());
    source->InsertEndChild(float_array);

    XMLElement* common = dae.NewElement("technique_common");
    source->InsertEndChild(common);

    XMLElement* accesor = makeAccesor(dae, "#weights_array", weights_count, "1");
    common->InsertEndChild(accesor);

    accesor->InsertEndChild( makeParam(dae, "WEIGHT", "float") );
}

void dhdm::Mesh::insertVertexWeights(XMLDocument & dae, XMLElement* parent) {
    XMLElement* joints = dae.NewElement("joints");
    parent->InsertEndChild(joints);

    joints->InsertEndChild( makeInput(dae, "JOINT", "#joints") );
    joints->InsertEndChild( makeInput(dae, "INV_BIND_MATRIX", "#bind_poses") );

    XMLElement* vertex_weights = dae.NewElement("vertex_weights");
    vertex_weights->SetAttribute("count", vweights.size());
    parent->InsertEndChild(vertex_weights);

    vertex_weights->InsertEndChild( makeInput(dae, "JOINT", "#joints", "0") );
    vertex_weights->InsertEndChild( makeInput(dae, "WEIGHT", "#weights", "1") );

    XMLElement* vcount = dae.NewElement("vcount");
    vertex_weights->InsertEndChild(vcount);

    std::ostringstream ss(std::ostringstream::out);
    for (auto & vw : vweights)
        ss << fmt::format("{} ", vw.weights.size());
    vcount->SetText(ss.str().c_str());

    XMLElement* v = dae.NewElement("v");
    vertex_weights->InsertEndChild(v);

    ss.str("");
    size_t n = 0;
    for (auto & vw : vweights) {
        for (auto it = vw.weights.begin(); it != vw.weights.end(); it++) {
            ss << fmt::format("{} {} ", it->first, n);
            n++;
        }
    }
    v->SetText(ss.str().c_str());
}

void dhdm::Mesh::insertSceneNodes( XMLDocument & dae, XMLElement* parent,
                                   const std::string & name, const short num_materials )
{
    std::string root_id;
    XMLElement* mesh_parent_node = parent;

    if (uses_vgroups) {
        XMLElement* armature_node = dae.NewElement("node");
        mesh_parent_node = armature_node;
        armature_node->SetAttribute("id", "ArmatureNode");
        armature_node->SetAttribute("name", "ColladaArmature");
        armature_node->SetAttribute("type", "NODE");
        parent->InsertEndChild(armature_node);

        if (vgroupsNames.size()>0) {
            const std::string vgroupName0_sid = vgroup_name_sid(vgroupsNames[0]);

            XMLElement* root_joint = dae.NewElement("node");
            root_id = "armature_" + vgroupName0_sid;
            root_joint->SetAttribute("id", root_id.c_str());
            root_joint->SetAttribute("name", vgroupsNames[0].c_str());
            root_joint->SetAttribute("sid", vgroupName0_sid.c_str());
            root_joint->SetAttribute("type", "JOINT");
            armature_node->InsertEndChild(root_joint);

            for (size_t i = 1; i < vgroupsNames.size(); i++) {
                const std::string vgroupName_sid = vgroup_name_sid(vgroupsNames[i]);

                XMLElement* joint = dae.NewElement("node");
                std::string joint_id = "armature_" + vgroupName_sid;
                joint->SetAttribute("id", joint_id.c_str());
                joint->SetAttribute("name", vgroupsNames[i].c_str());
                joint->SetAttribute("sid", vgroupName_sid.c_str());
                joint->SetAttribute("type", "JOINT");
                root_joint->InsertEndChild(joint);
            }
        }
    }

    XMLElement* mesh_node = dae.NewElement("node");
    mesh_node->SetAttribute("id", "MeshNode");
    mesh_node->SetAttribute("name", name.c_str());
    mesh_node->SetAttribute("type", "NODE");
    mesh_parent_node->InsertEndChild(mesh_node);

    XMLElement* instance_material_parent;
    if ( !uses_vgroups )
    {
        XMLElement* instance_geometry = dae.NewElement("instance_geometry");
        instance_geometry->SetAttribute("url", "#mesh");
        instance_geometry->SetAttribute("name", name.c_str());
        mesh_node->InsertEndChild(instance_geometry);
        instance_material_parent = instance_geometry;
    }
    else
    {
        XMLElement* instance_controller = dae.NewElement("instance_controller");
        instance_controller->SetAttribute("url", "#armature");
        mesh_node->InsertEndChild(instance_controller);
        XMLElement* skeleton = dae.NewElement("skeleton");
        skeleton->SetText(std::string("#" + root_id).c_str());
        instance_controller->InsertEndChild(skeleton);
        instance_material_parent = instance_controller;
    }

    if (uses_materials) {
        XMLElement* bind_material = dae.NewElement("bind_material");
        instance_material_parent->InsertEndChild(bind_material);
        XMLElement* common = dae.NewElement("technique_common");
        bind_material->InsertEndChild(common);
        for (short i=0; i < num_materials; i++) {
            XMLElement* instance_material = dae.NewElement("instance_material");
            instance_material->SetAttribute("symbol", fmt::format("mat{}", i).c_str());
            instance_material->SetAttribute("target", fmt::format("#mat{}", i).c_str());
            common->InsertEndChild(instance_material);
        }
    }
}


void dhdm::Mesh::writeCollada(const std::string & fp, const std::string & name)
{
    std::cout << "Writing .dae...\n";
    XMLDocument dae;

    XMLDeclaration* decl = dae.NewDeclaration();
    dae.InsertFirstChild(decl);

    XMLElement* collada = dae.NewElement("COLLADA");
    collada->SetAttribute("xmlns", "http://www.collada.org/2005/11/COLLADASchema");
    collada->SetAttribute("version", "1.4.1");
    collada->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    dae.InsertEndChild(collada);

    XMLElement* asset = dae.NewElement("asset");
    XMLElement* unit = dae.NewElement("unit");
    unit->SetAttribute("name", "meter");
    unit->SetAttribute("meter", 1);
    asset->InsertEndChild(unit);
    XMLElement* up_axis = dae.NewElement("up_axis");
    up_axis->SetText("Z_UP");
    asset->InsertEndChild(up_axis);
    collada->InsertEndChild(asset);

    std::vector< std::vector<Face> > materials_faces;
    if (uses_materials) {
        for (auto & face : faces) {
            // assert(face.matId >= 0);
            if ( (size_t) face.matId >= materials_faces.size())
                materials_faces.resize(face.matId + 1);
            materials_faces[face.matId].push_back(face);
        }
        XMLElement* library_materials = dae.NewElement("library_materials");
        collada->InsertEndChild(library_materials);
        for (size_t i=0; i < materials_faces.size(); i++) {
            XMLElement* material = dae.NewElement("material");
            material->SetAttribute("id", fmt::format("mat{}", i).c_str());
            material->SetAttribute("name", fmt::format("SLOT_{}", i).c_str());
            library_materials->InsertEndChild(material);
        }
    }

    std::vector<std::string> uv_layers_names;
    if (uses_uvs) {
        for (size_t i=0; i < uv_layers.size(); i++)
            uv_layers_names.push_back(fmt::format("uv_map_{}", i));
    }

    XMLElement* library_geometries = dae.NewElement("library_geometries");
    collada->InsertEndChild(library_geometries);

    XMLElement* geometry = dae.NewElement("geometry");
    geometry->SetAttribute("id", "mesh");
    geometry->SetAttribute("name", name.c_str());
    library_geometries->InsertEndChild(geometry);

    XMLElement* mesh = dae.NewElement("mesh");
    geometry->InsertEndChild(mesh);

    insertVerticesPosSource(dae, mesh);
    insertUVsSource(dae, mesh, uv_layers_names);
    insertPolylist(dae, mesh, materials_faces, uv_layers_names);
    const short num_materials = (short) materials_faces.size();
    materials_faces.clear();

    if ( uses_vgroups )
    {
        XMLElement* library_controllers = dae.NewElement("library_controllers");
        collada->InsertEndChild(library_controllers);

        XMLElement* armature = dae.NewElement("controller");
        armature->SetAttribute("id", "armature");
        armature->SetAttribute("name", name.c_str());
        library_controllers->InsertEndChild(armature);

        XMLElement* skin = dae.NewElement("skin");
        skin->SetAttribute("source", "#mesh");
        armature->InsertEndChild(skin);

        insertJointsSource(dae, skin);
        insertBindPosesSource(dae, skin);
        insertWeightsSource(dae, skin);
        insertVertexWeights(dae, skin);
    }

    XMLElement* library_visual_scenes = dae.NewElement("library_visual_scenes");
    collada->InsertEndChild(library_visual_scenes);
    XMLElement* lib_scene = dae.NewElement("visual_scene");
    lib_scene->SetAttribute("id", "Scene");
    lib_scene->SetAttribute("name", "Scene");
    library_visual_scenes->InsertEndChild(lib_scene);

    insertSceneNodes( dae, lib_scene, name, num_materials );

    XMLElement* scene = dae.NewElement("scene");
    collada->InsertEndChild(scene);
    XMLElement* instance_scene = dae.NewElement("instance_visual_scene");
    instance_scene->SetAttribute("url", "#Scene");
    scene->InsertEndChild(instance_scene);

    dae.SaveFile(fp.c_str());
    std::cout << "Finished writing .dae.\n";
}

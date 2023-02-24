#include <iostream>
#include <fmt/core.h>

#include "mesh.hh"
#include "utils.hh"


using namespace tinyxml2;

/*
    load_uv_layers:
        -1 : none
         0 : all
        n>0: only layer n-1
*/
dhdm::Mesh dhdm::Mesh::fromDae( const char * fp_dae,
                                const char * fp_obj,
                                const short load_uv_layers,
                                bool load_materials,
                                bool load_vgroups )
{
    std::cout << "Reading file \"" << std::string(fp_dae) << "\"...\n";

    const bool has_facemap = (fp_obj != nullptr);
    dhdm::Mesh mesh;
    FaceMap fm;
    if (has_facemap)
    {
        fm = mesh.faceMapfromObj(fp_obj);
        // std::cout << fmt::format("Number of faces: {}\n", mesh.faces.size());
    }

    XMLDocument dae;
	dae.LoadFile( fp_dae );

    const XMLElement* const e_collada = dae.FirstChildElement("COLLADA");
    const XMLElement* e_mesh = e_collada->FirstChildElement("library_geometries")->
                                          FirstChildElement("geometry")->
                                          FirstChildElement("mesh");

    bool loaded_vertices = false;
    size_t mat_id = 0;

    const bool load_uvs = ( load_uv_layers >=0 );
    const short layer_to_load = load_uv_layers - 1;
    const std::string id_uv_u = "-mesh-map";
    const std::string id_uv_n_pref = "-mesh-map-";
    std::string id_uv;
    if ( layer_to_load >= 0 )
        id_uv = id_uv_n_pref + std::to_string(layer_to_load);
    else
        id_uv = id_uv_n_pref + std::string("0");

    const XMLElement* elem = e_mesh->FirstChildElement();
    while ( elem != nullptr )
    {
        /*
        std::cout << "---\n";
        std::cout << elem->Name() << std::endl;
        const XMLAttribute * att = elem->FirstAttribute();
        std::cout << "  " << att->Name() << " : " << att->Value() << std::endl;
        */

        // vertices
        if ( !loaded_vertices &&
             strcmp(elem->Name(), "source") == 0 )
        {
            const char* id = elem->Attribute("id");
            if ( id != nullptr && endswith(std::string(id), "-mesh-positions") )
            {
                const XMLElement* e_float_array = elem->FirstChildElement("float_array");
                if (e_float_array != nullptr)
                {
                    NumberReader<double> nr( e_float_array->GetText() );
                    while ( nr.has_next() )
                    {
                        dhdm::Vertex vert;
                        vert.pos[0] = nr.read_next();
                        vert.pos[1] = nr.read_next();
                        vert.pos[2] = nr.read_next();

                        vert.pos = vert.pos * (1/dhdm::gScale);
                        mesh.vertices.push_back( std::move(vert) );
                    }

                    loaded_vertices = true;
                }
            }
        }
        // uvs
        else if ( load_uvs &&
                  strcmp(elem->Name(), "source") == 0 )
        {
            const char* id = elem->Attribute("id");
            if ( id != nullptr &&
                 ( endswith(std::string(id), id_uv) ||
                   endswith(std::string(id), id_uv_u) ) )
            {
                const XMLElement* e_float_array = elem->FirstChildElement("float_array");
                if (e_float_array != nullptr)
                {
                    std::vector<dhdm::UV> uvs;

                    NumberReader<double> nr( e_float_array->GetText() );
                    while ( nr.has_next() )
                    {
                        dhdm::UV uv;
                        uv.pos[0] = nr.read_next();
                        uv.pos[1] = nr.read_next();

                        uvs.push_back( std::move(uv) );
                    }

                    mesh.uv_layers.push_back(std::move(uvs));
                }
                id_uv = id_uv_n_pref + std::to_string(mesh.uv_layers.size());
            }
        }
        // faces/materials
        else if ( strcmp(elem->Name(), "polylist") == 0 )
        {
            const char* material_name = elem->Attribute("material");
            if (material_name != nullptr)
            {
                const XMLElement* e_vcount = elem->FirstChildElement("vcount");
                const XMLElement* e_p = elem->FirstChildElement("p");
                if (e_p != nullptr && e_vcount != nullptr)
                {
                    NumberReader<unsigned long> nr_vc( e_vcount->GetText() );
                    NumberReader<unsigned long> nr_p( e_p->GetText() );

                    while ( nr_vc.has_next() )
                    {
                        unsigned long n_vcount = nr_vc.read_next();
                        if ( n_vcount != 4 && n_vcount != 3 )
                            throw std::runtime_error("Unsupported face (isn't a triangle or quad)");
                        if (has_facemap)
                        {
                            unsigned int vs[4];
                            unsigned int uvs[4];
                            for (unsigned long i = 0; i < n_vcount; i++)
                            {
                                vs[i] = nr_p.read_next() + 1;
                                nr_p.read_next();
                                uvs[i] = nr_p.read_next();
                            }

                            if (n_vcount == 3)
                                vs[3] = 0;

                            auto it = fm.find( FaceTuple(vs[0], vs[1], vs[2], vs[3]) );
                            if (it == fm.end() )
                            {
                                std::cout << "Face: (" << vs[0] << ", " << vs[1] << ", " << vs[2] << ", " << vs[3] << ") not found.\n";
                                throw std::runtime_error("Face not found.");
                            }

                            Face & f = mesh.faces[it->second];

                            f.matId = mat_id;
                            for (unsigned long i = 0; i < n_vcount; i++)
                                f.vertices[i].uv = uvs[i];
                        }
                        else
                        {
                            dhdm::Face face;
                            face.matId = mat_id;
                            for (unsigned long i = 0; i < n_vcount; i++)
                            {
                                dhdm::FaceVertex fv;

                                fv.vertex = nr_p.read_next();
                                nr_p.read_next();
                                fv.uv = nr_p.read_next();

                                face.vertices.push_back( std::move(fv) );
                            }
                            mesh.faces.push_back( std::move(face) );
                        }

                    }
                }
                mat_id++;
            }
        }

        elem = elem->NextSiblingElement();
    }
    mesh.uses_uvs = load_uvs;
    mesh.uses_materials = load_materials;

    // weights
    if (load_vgroups) {
        const XMLElement* elem = e_collada->FirstChildElement("library_controllers");
        if (elem == nullptr)
        {
            load_vgroups = false;
        }
        else
        {
            elem = elem->FirstChildElement("controller")->
                        FirstChildElement("skin")->
                        FirstChildElement();
            std::vector<double> weights;
            while ( elem != nullptr )
            {
                /*
                std::cout << "---\n";
                std::cout << elem->Name() << std::endl;
                const XMLAttribute * att = elem->FirstAttribute();
                std::cout << "  " << att->Name() << " : " << att->Value() << std::endl;
                */

                if ( strcmp(elem->Name(), "source") == 0 )
                {
                    const char* id = elem->Attribute("id");
                    if ( id != nullptr )
                    {
                        /*
                        std::cout << "id: " << std::string(id) << std::endl;
                        */
                        if ( endswith( std::string(id), "-skin-joints" ) )
                        {
                            const XMLElement* e_name_array = elem->FirstChildElement("Name_array");
                            if (e_name_array != nullptr)
                            {
                                mesh.vgroupsNames = split( std::string( e_name_array->GetText() ), " " );
                            }
                        }
                        else if ( endswith( std::string(id), "-skin-weights" ) )
                        {
                            const XMLElement* e_float_array = elem->FirstChildElement("float_array");
                            if (e_float_array != nullptr)
                            {
                                NumberReader<double> nr( e_float_array->GetText() );
                                while ( nr.has_next() )
                                    weights.push_back( nr.read_next() );
                            }
                        }
                    }
                }
                else if ( strcmp(elem->Name(), "vertex_weights") == 0 )
                {
                    const XMLElement* e_vcount = elem->FirstChildElement("vcount");
                    const XMLElement* e_v = elem->FirstChildElement("v");

                    if (e_vcount != nullptr && e_v != nullptr )
                    {
                        unsigned long max_joint = 0;
                        NumberReader<unsigned long> nr_vcount( e_vcount->GetText() );
                        NumberReader<unsigned long> nr_v( e_v->GetText() );

                        while ( nr_vcount.has_next() )
                        {
                            dhdm::VertexWeights vw;

                            unsigned long n_vcount = nr_vcount.read_next();
                            for (unsigned long i = 0; i < n_vcount; i++)
                            {
                                const unsigned long i_joint = nr_v.read_next();
                                const unsigned long i_w = nr_v.read_next();

                                if ( i_w >= weights.size() )
                                {
                                    const std::string tmp = std::string("Invalid .collada file (vertex weights) \"") +
                                                            std::string(fp_dae) + "\"";
                                    throw std::runtime_error(tmp);
                                }

                                vw.weights[ i_joint ] = weights[ i_w ];
                                max_joint = std::max(max_joint, i_joint);
                            }
                            mesh.vweights.push_back( std::move(vw) );
                        }

                        if ( max_joint >= mesh.vgroupsNames.size() )
                        {
                            const std::string tmp = std::string("Invalid .collada file (joints) \"") +
                                                    std::string(fp_dae) + "\"";
                            throw std::runtime_error(tmp);
                        }
                    }
                }
                elem = elem->NextSiblingElement();
            }
        }
    }
    mesh.uses_vgroups = load_vgroups;

    /*
    std::cout << "-----\n";
    std::cout << "Vertices: " << mesh.vertices.size() << std::endl;
    for (size_t i = 0; i < 3 && i < mesh.vertices.size(); i++)
        print_vertex(mesh.vertices, i);
    print_vertex(mesh.vertices, mesh.vertices.size()-1);
    print_vertex(mesh.vertices, mesh.vertices.size()-2);
    std::cout << "---\n";

    std::cout << "Faces: " << mesh.faces.size() << std::endl;
    std::cout << "UV layers: " << mesh.uv_layers.size() << std::endl;
    if ( mesh.uv_layers.size() > 0 )
        std::cout << "UV layer 0: " << mesh.uv_layers[0].size() << std::endl;

    std::cout << "Joints: " << mesh.vgroupsNames.size() << std::endl;
    std::cout << "Weights: " << mesh.vweights.size() << std::endl;
    std::cout << "-----\n";
    */

    std::cout << "Finished reading .dae.\n";
    return mesh;
}


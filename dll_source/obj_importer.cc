#include <optional>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <glm/gtx/io.hpp>
#include <fmt/format.h>

#include "mesh.hh"
#include "utils.hh"



FaceMap dhdm::Mesh::faceMapfromObj( const char * fp_obj )
{
    std::cout << "Reading file \"" << std::string(fp_obj) << "\"...\n";

    auto fs = std::fstream(fp_obj, std::fstream::in);
    if (!fs) throw std::runtime_error("cannot open file");

    FaceMap fm;
    unsigned int n_face = 0;
    std::string line;
    while (std::getline(fs, line))
    {
        if (std::string_view(line).substr(0, 2) == "f ")
        {
            uint32_t vs[4];
            int c = sscanf(line.c_str() + 2, "%d %d %d %d", vs, vs+1, vs+2, vs+3);
            if ( c != 4 && c != 3 )
                throw std::runtime_error("Invalid face: " + line);
            if ( c == 3 )
                vs[3] = 0;
            Face face;
            for (int i=0; i < c; i++)
            {
                FaceVertex fv;
                fv.vertex = vs[i]-1;
                face.vertices.push_back( std::move(fv) );
            }

            faces.push_back( std::move(face) );
            FaceTuple f( vs[0], vs[1], vs[2], vs[3] );
            fm[ f ] = n_face;
            n_face++;
        }
    }

    std::cout << "Finished reading .obj.\n";
    return fm;
}


dhdm::Mesh dhdm::Mesh::fromObj( const std::string & fp,
                                const bool load_uvs,
                                const bool load_materials,
                                const bool use_face_id_mat_id )
{
    std::cout << "Reading file \"" << fp << "\"...\n";
    auto fs = std::fstream(fp, std::fstream::in);
    if (!fs)
        throw std::runtime_error("cannot open file");

    dhdm::Mesh mesh;

    std::optional<std::string> objectName;

    std::unordered_map<std::string, short> materialMap;
    short curr_matId = -1;

    bool first_face = true;
    std::vector<UV> uv_layer;

    std::string line;
    while (std::getline(fs, line))
    {
        if ( std::string_view(line).substr(0, 2) == "o " ) {
            if (objectName)
                throw std::runtime_error(".obj file contains multiple meshes.");
            objectName = std::string(line, 2);

        } else if (std::string_view(line).substr(0, 2) == "v ") {
            double x, y, z;
            if ( sscanf(line.c_str() + 2, "%lf %lf %lf", &x, &y, &z) != 3 )
                throw std::runtime_error("Invalid vertex: " + line);
            dhdm::Vertex vert( { glm::dvec3(x, y, z) } );
            vert.pos = vert.pos * (1/dhdm::gScale);
            mesh.vertices.push_back( std::move(vert) );

        } else if (std::string_view(line).substr(0, 3) == "vt ") {
            if (load_uvs)
            {
                double u, v;
                if (sscanf(line.c_str() + 3, "%lf %lf", &u, &v) != 2)
                    throw std::runtime_error("Invalid texture coordinate: " + line);
                uv_layer.push_back({ glm::dvec2(u, v) });
            }
        } else if (std::string_view(line).substr(0, 2) == "f ") {
            Face face;
            if (load_uvs)
            {
                uint32_t vs[4];
                uint32_t uvs[4];
                int c = sscanf(line.c_str() + 2, "%d/%d %d/%d %d/%d %d/%d", vs, uvs, vs+1, uvs+1, vs+2, uvs+2, vs+3, uvs+3);
                if ( c != 6 && c != 8 )
                    throw std::runtime_error("Invalid face: " + line);
                c /= 2;
                for (int i=0; i < c; i++)
                    face.vertices.push_back( { .vertex = VertexId(vs[i]-1), .uv = UvId(uvs[i]-1) } );
            }
            else
            {
                uint32_t vs[4];
                const int c = sscanf(line.c_str() + 2, "%d %d %d %d", vs, vs+1, vs+2, vs+3);
                if ( c != 3 && c != 4 )
                    throw std::runtime_error("Invalid face: " + line);
                for (int i=0; i < c; i++)
                    face.vertices.push_back( { .vertex = VertexId(vs[i]-1), .uv = 0 } );
            }

            if (load_materials)
            {
                face.matId = curr_matId;
            }
            else if (use_face_id_mat_id)
            {
                face.matId = mesh.faces.size();
            }
            else
            {
                face.matId = 0;
            }

            mesh.faces.push_back( std::move(face) );

        } else if (load_materials && std::string_view(line).substr(0, 7) == "usemtl ") {
            std::string matName = std::string(line, 7);

            auto it = materialMap.find(matName);
            if (it != materialMap.end()) {
                curr_matId = it -> second;
            } else {
                mesh.materialNames.push_back(matName);
                curr_matId = (short) (mesh.materialNames.size() - 1);
                materialMap[matName] = curr_matId;
            }
        }

    }

    mesh.uses_uvs = false;
    if (load_uvs && uv_layer.size() > 0)
    {
        mesh.uv_layers.push_back(std::move(uv_layer));
        mesh.uses_uvs = true;
    }
    mesh.uses_materials = false;
    if (load_materials && mesh.materialNames.size() > 0)
        mesh.uses_materials = true;

    mesh.uses_vgroups = false;

    /*
    std::cout << "Mesh name: " << *objectName << "\n";
    std::cout << fmt::format("Number of vertices: {}\n", mesh.vertices.size());
    std::cout << fmt::format("Number of faces: {}\n", mesh.faces.size());
    std::cout << fmt::format("Number of materials: {}\n", mesh.materialNames.size());
    */

    std::cout << "Finished reading .obj.\n";
    return mesh;
}

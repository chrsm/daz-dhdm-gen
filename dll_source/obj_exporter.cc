#include <optional>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <glm/gtx/io.hpp>
#include <fmt/format.h>

#include "mesh.hh"
#include "utils.hh"


void dhdm::Mesh::writeObj(const std::string & fp)
{
    std::cout << "Writing .obj...\n";
    std::ofstream outf;
    outf.open( fp, std::ofstream::out | std::ofstream::trunc );

    for (auto & v : vertices)
    {
        const glm::dvec3 pos = v.pos * dhdm::gScale;
        outf << fmt::format("v {} {} {}\n", (float) pos.x, (float) pos.y, (float) pos.z);
    }

    if (uses_uvs && uv_layers.size() > 0) {
        for (auto & uv : uv_layers[0])
            outf << fmt::format("vt {} {}\n", (float) uv.pos.x, (float) uv.pos.y);
    }

    if (!uses_materials) {
        if (uses_uvs)
        {
            for (auto & f : faces) {
                outf << "f";
                for (auto & fv : f.vertices)
                    outf << fmt::format(" {}/{}", fv.vertex + 1, fv.uv + 1);
                outf << "\n";
            }
        }
        else
        {
            for (auto & f : faces) {
                outf << "f";
                for (auto & fv : f.vertices)
                    outf << fmt::format(" {}", fv.vertex + 1);
                outf << "\n";
            }
        }
    }
    else {
        std::vector< std::vector<Face> > materials_faces;
        for (auto & f : faces) {
            if ( (size_t) f.matId >= materials_faces.size())
                materials_faces.resize(f.matId + 1);
            materials_faces[f.matId].push_back(f);
        }

        const bool has_material_names = (materialNames.size() > 0);

        for (size_t i=0; i < materials_faces.size(); i++) {
            if (has_material_names)
                outf << fmt::format("usemtl {}\n", materialNames[i]);
            else
                outf << fmt::format("usemtl SLOT_{}\n", i);

            for (auto & f : materials_faces[i]) {
                outf << "f";
                if (uses_uvs)
                {
                    for (auto & fv : f.vertices)
                        outf << fmt::format(" {}/{}", fv.vertex + 1, fv.uv + 1);
                }
                else
                {
                    for (auto & fv : f.vertices)
                        outf << fmt::format(" {}", fv.vertex + 1);
                }
                outf << "\n";
            }
        }
    }

    outf.close();
    std::cout << "Finished writing .obj.\n";
}

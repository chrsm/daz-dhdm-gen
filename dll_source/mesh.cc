#include <optional>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <glm/gtx/io.hpp>
#include <fmt/format.h>

#include "mesh.hh"
#include "utils.hh"


double dhdm::gScale = 0.01;


void dhdm::Mesh::triangulate()
{
    auto nrFaces = faces.size();
    faces.reserve(nrFaces * 2);
    for (size_t i = 0; i < nrFaces; ++i) {
        auto & face = faces[i];
        if (face.vertices.size() == 3) continue;
        // assert(face.vertices.size() == 4);
        faces.push_back(Face { .vertices = { face.vertices[2], face.vertices[3], face.vertices[0] } });
        face.vertices.pop_back();
    }
}

void dhdm::Mesh::set_subd_only_deltas(const dhdm::Mesh * originalMesh)
{
    this->originalMesh = originalMesh;
    subd_only_deltas = true;
}


#include <iostream>
#include <fstream>
#include <map>
#include <fmt/format.h>

#include "mesh.hh"
#include "utils.hh"


template <class T>
std::optional<typename T::mapped_type> get(const T & map, const typename T::key_type & key)
{
    auto i = map.find(key);
    if (i == map.end()) return {};
    return std::optional<typename T::mapped_type>(i->second);
}


dhdm::Mesh dhdm::Mesh::fromDSF(const std::string & geoFile, const std::string & uvFile)
{
    dhdm::Mesh mesh;

    std::map<std::pair<FaceId, VertexId>, UvId> overrides;

    {
        std::vector<UV> uv_layer;
        const auto uvMap = readJSON(uvFile)["uv_set_library"][0];
        for (auto & uv : uvMap["uvs"]["values"]) {
            assert(uv.size() == 2);
            uv_layer.push_back({glm::dvec2(uv[0], uv[1])});
        }
        mesh.uv_layers.push_back(std::move(uv_layer));

        for (auto & p : uvMap["polygon_vertex_indices"]) {
            assert(p.size() == 3);
            overrides.insert_or_assign({p[0], p[1]}, p[2]);
        }
    }

    const auto geometry = readJSON(geoFile)["geometry_library"][0];

    for (auto & vertex : geometry["vertices"]["values"]) {
        assert(vertex.size() == 3);
        mesh.vertices.push_back({glm::dvec3(vertex[0], -(double) vertex[2], vertex[1])});
    }

    for (auto & poly : geometry["polylist"]["values"]) {
        assert(poly.size() >= 5);
        std::vector<FaceVertex> vertices;
        dhdm::FaceId faceIdx = mesh.faces.size();
        for (size_t i = 2; i < poly.size(); ++i) {
            VertexId vertexIdx = poly[i];
            assert(vertexIdx < mesh.vertices.size());
            auto uvIdx = get(overrides, {faceIdx, vertexIdx}).value_or(vertexIdx);
            vertices.push_back({ .vertex = vertexIdx, .uv = uvIdx });
        }
        mesh.faces.push_back({ .vertices = std::move(vertices) });
    }

    std::cout << fmt::format( "Read {}: {} vertices, {} faces, {} UVs\n",
                              geoFile, mesh.vertices.size(), mesh.faces.size(),
                              mesh.uv_layers[0].size() );

    return mesh;
}


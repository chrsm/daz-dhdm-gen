#include <iostream>
#include <fmt/format.h>

#include "mesh.hh"


using namespace OpenSubdiv;


void dhdm::Mesh::subdivide_nonvertex( const unsigned int level,
                                      std::unique_ptr<Far::TopologyRefiner> & refiner,
                                      Far::PrimvarRefiner & primvarRefiner )
{
    std::vector<short> matIdbuffer;
    if (uses_materials)
    {
        const int last_level_faces = refiner->GetLevel(level).GetNumFaces();
        matIdbuffer.resize( last_level_faces );

        std::vector<short> matIdbuffer_pv( refiner->GetNumFacesTotal() - last_level_faces );
        for ( size_t i=0; i < faces.size(); i++ )
            matIdbuffer_pv[i] = faces[i].matId;

        short * srcFUnifMat = matIdbuffer_pv.data();
        int face_offset = faces.size();

        for (unsigned int lvl = 1; lvl <= level; ++lvl)
        {
            auto dstFUnifMat = (lvl == level) ? matIdbuffer.data() : (matIdbuffer_pv.data() + face_offset);
            primvarRefiner.InterpolateFaceUniform(lvl, srcFUnifMat, dstFUnifMat);
            srcFUnifMat = dstFUnifMat;
            face_offset += refiner->GetLevel(lvl).GetNumFaces();
        }
    }

    if (uses_vgroups)
    {
        const int verts_total = refiner->GetNumVerticesTotal();
        const int last_level_verts = refiner->GetLevel(level).GetNumVertices();

        std::vector<dhdm::VertexWeights> vweightsbuffer_pv(verts_total - vweights.size() - last_level_verts);
        std::vector<dhdm::VertexWeights> vweightsbuffer(last_level_verts);

        dhdm::VertexWeights * srcWeights = vweights.data();
        int vert_offset = 0;
        for (unsigned int lvl = 1; lvl <= level; ++lvl)
        {
            auto dstWeights = (lvl == level) ? vweightsbuffer.data() : (vweightsbuffer_pv.data() + vert_offset);
            primvarRefiner.Interpolate(lvl, srcWeights, dstWeights);
            srcWeights = dstWeights;
            vert_offset += refiner->GetLevel(lvl).GetNumVertices();
        }

        vweights = std::move(vweightsbuffer);
    }
    else
    {
        vweights.clear();
    }

    if (uses_uvs)
    {
        const int fvars_total = refiner->GetNumFVarValuesTotal(0);
        const int last_level_fvars = refiner->GetLevel(level).GetNumFVarValues(0);

        for (size_t i = 0; i < uv_layers.size(); i++)
        {
            std::vector<dhdm::UV> uv_layer_buffer( last_level_fvars );
            std::vector<dhdm::UV> uvbuffer_pv( fvars_total - uv_layers[i].size() - last_level_fvars );
            const dhdm::UV* srcFVarUV = uv_layers[i].data();

            int fvar_offset = 0;
            for (unsigned int lvl = 1; lvl <= level; lvl++) {
                dhdm::UV * dstFVarUV = (lvl == level) ? uv_layer_buffer.data() : (uvbuffer_pv.data() + fvar_offset);
                primvarRefiner.InterpolateFaceVarying(lvl, srcFVarUV, dstFVarUV, 0);
                srcFVarUV = dstFVarUV;
                fvar_offset += refiner->GetLevel(lvl).GetNumFVarValues(0);
            }

            uv_layers[i] = std::move(uv_layer_buffer);
        }
    }
    else
    {
        uv_layers.clear();
    }

    faces.clear();
    auto lastLevel = refiner->GetLevel(level);
    const size_t last_level_faces = lastLevel.GetNumFaces();

    for (size_t i = 0; i < last_level_faces; i++)
    {
        std::vector<FaceVertex> fvs;
        const auto fverts = lastLevel.GetFaceVertices(i);

        if (uses_uvs)
        {
            const auto fvuvs = lastLevel.GetFaceFVarValues(i, 0);
            for (int j = 0; j < fverts.size(); j++)
                fvs.push_back({ .vertex = (VertexId) fverts[j], .uv = (UvId) fvuvs[j] });
        }
        else
        {
            for (int j = 0; j < fverts.size(); j++)
                fvs.push_back({ .vertex = (VertexId) fverts[j], .uv = (UvId) 0 });
        }

        const short matId = uses_materials ? matIdbuffer[i] : 0;
        faces.push_back({ .vertices = std::move(fvs), .matId = matId });
    }
}


void dhdm::Mesh::subdivide_simple( const unsigned int level )
{
    if (level == 0) return;
    std::cout << fmt::format("Subdividing (simple) to level {}...\n", level);

    std::unique_ptr<Far::TopologyRefiner> refiner( dhdm::createTopologyRefiner( level, *this ) );
    Far::PrimvarRefiner primvarRefiner(*refiner);

    subdivide_nonvertex(level, refiner, primvarRefiner);

    const int last_level_verts = refiner->GetLevel(level).GetNumVertices();
    std::vector<dhdm::Vertex> vbuffer_pv( refiner->GetNumVerticesTotal() - vertices.size() - last_level_verts );
    std::vector<dhdm::Vertex> vbuffer( last_level_verts );
    dhdm::Vertex * srcVerts = vertices.data();
    int vert_offset = 0;

    for (unsigned int lvl = 1; lvl <= level; ++lvl) {
        auto dstVerts = (lvl == level) ? vbuffer.data() : (vbuffer_pv.data() + vert_offset);
        primvarRefiner.Interpolate(lvl, srcVerts, dstVerts);
        srcVerts = dstVerts;
        vert_offset += refiner->GetLevel(lvl).GetNumVertices();
    }

    vertices = std::move(vbuffer);
}

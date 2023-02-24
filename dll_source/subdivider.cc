#include <iostream>
#include <fmt/format.h>

#include "mesh.hh"


using namespace OpenSubdiv;


Far::TopologyRefiner * dhdm::createTopologyRefiner( const unsigned int level, const Mesh & baseMesh )
{
    std::vector<int> vertsPerFace;
    std::vector<int> vertIndices;
    std::vector<int> uvIndices;

    for (auto & face : baseMesh.faces) {
        const int num_face_verts = face.vertices.size();

        vertsPerFace.push_back(num_face_verts);
        for (auto & vert : face.vertices) {
            vertIndices.push_back(vert.vertex);
            if (baseMesh.uses_uvs)
                uvIndices.push_back(vert.uv);
        }

        if (num_face_verts != 4 && num_face_verts != 3)
            throw std::runtime_error("Mesh must have triangles or quads only");
    }

    typedef Far::TopologyDescriptor Descriptor;
    Sdc::SchemeType type = OpenSubdiv::Sdc::SCHEME_CATMARK;

    Sdc::Options options;
    options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);

    Descriptor desc;
    desc.numVertices = baseMesh.vertices.size();
    desc.numFaces = baseMesh.faces.size();
    desc.numVertsPerFace = vertsPerFace.data();
    desc.vertIndicesPerFace = vertIndices.data();

    Descriptor::FVarChannel uvChannel;
    if (baseMesh.uses_uvs) {
        if (baseMesh.uv_layers.size() == 0)
            throw std::runtime_error("Empty uv_layers with uses_uvs");
        options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_ONLY);
        uvChannel.numValues = baseMesh.uv_layers[0].size();
        uvChannel.valueIndices = uvIndices.data();
        desc.numFVarChannels = 1;
        desc.fvarChannels = &uvChannel;
    }

    Far::TopologyRefiner * refiner =
            Far::TopologyRefinerFactory<Descriptor>::Create(
                desc,
                Far::TopologyRefinerFactory<Descriptor>::Options(type, options)
            );

    Far::TopologyRefiner::UniformOptions refineOptions(level);
    refineOptions.fullTopologyInLastLevel = false; // true;
    refiner->RefineUniform(refineOptions);

    return refiner;
}


void dhdm::MeshSubdivider::subdivide_simple_init()
{
    Far::StencilTableFactory::Options options;
    options.generateIntermediateLevels = false;
    //options.factorizeIntermediateLevels = false;
    options.generateOffsets = true;

    options.interpolationMode = Far::StencilTableFactory::INTERPOLATE_VERTEX;
    // options.interpolationMode = Far::StencilTableFactory::INTERPOLATE_VARYING;
    // options.interpolationMode = Far::StencilTableFactory::INTERPOLATE_FACE_VARYING;

    vertexStencils.reset( Far::StencilTableFactory::Create(*refiner, options) );

    vbuffer.resize( vertexStencils->GetNumStencils() );
    // assert( vbuffer.size() == refiner->GetLevel(level).GetNumVertices() );

    is_simple_init = true;
}

dhdm::MeshSubdivider::MeshSubdivider( const dhdm::Mesh * baseMesh, const unsigned int level ) :
    level(level), uses_uvs(baseMesh->uses_uvs),
    uses_vgroups(baseMesh->uses_vgroups), uses_materials(baseMesh->uses_materials),
    baseMesh(baseMesh)
{
    if (level == 0)
        throw std::runtime_error("MeshSubdividerSimple() with level == 0");

    refiner.reset( createTopologyRefiner(level, *baseMesh) );

    if ( uses_uvs || uses_materials || uses_vgroups )
    {
        Far::PrimvarRefiner primvarRefiner(*refiner);

        if (uses_vgroups) {
            const int last_level_vertices = refiner->GetLevel(level).GetNumVertices();
            vweightsbuffer.resize(last_level_vertices);
            std::vector<dhdm::VertexWeights> vweightsbuffer_pv( refiner->GetNumVerticesTotal() - baseMesh->vertices.size() - last_level_vertices );
            const dhdm::VertexWeights * srcWeights = baseMesh->vweights.data();
            int vert_offset = 0;

            for (unsigned int lvl = 1; lvl <= level; lvl++) {
                dhdm::VertexWeights* dstWeights = (lvl == level) ? vweightsbuffer.data() : (vweightsbuffer_pv.data() + vert_offset);
                primvarRefiner.Interpolate(lvl, srcWeights, dstWeights);
                srcWeights = dstWeights;
                vert_offset += refiner->GetLevel(lvl).GetNumVertices();
            }
        }

        if (uses_uvs) {
            const int last_level_fvars = refiner->GetLevel(level).GetNumFVarValues(0);
            const int fvars_total = refiner->GetNumFVarValuesTotal(0);

            for (size_t i = 0; i < baseMesh->uv_layers.size(); i++)
            {
                std::vector<dhdm::UV> uv_layer_buffer(last_level_fvars);
                std::vector<dhdm::UV> uvbuffer_pv( fvars_total - baseMesh->uv_layers[i].size() - last_level_fvars );
                const dhdm::UV* srcFVarUV = baseMesh->uv_layers[i].data();

                int fvar_offset = 0;
                for (unsigned int lvl = 1; lvl <= level; lvl++) {
                    dhdm::UV * dstFVarUV = (lvl == level) ? uv_layer_buffer.data() : (uvbuffer_pv.data() + fvar_offset);
                    primvarRefiner.InterpolateFaceVarying(lvl, srcFVarUV, dstFVarUV, 0);
                    srcFVarUV = dstFVarUV;
                    fvar_offset += refiner->GetLevel(lvl).GetNumFVarValues(0);
                }

                uv_layers_buffers.push_back(std::move(uv_layer_buffer));
            }
        }

        if (uses_materials) {
            const int last_level_faces = refiner->GetLevel(level).GetNumFaces();
            matIdbuffer.resize( last_level_faces );
            std::vector<short> matIdbuffer_pv( refiner->GetNumFacesTotal() - last_level_faces );
            for (size_t i=0; i < baseMesh->faces.size(); i++)
                matIdbuffer_pv[i] = faces[i].matId;
            short * srcFUnifMat = matIdbuffer_pv.data();
            int face_offset = baseMesh->faces.size();

            for (unsigned int lvl = 1; lvl <= level; ++lvl) {
                short * dstFUnifMat = (lvl == level) ? matIdbuffer.data() : (matIdbuffer_pv.data() + face_offset);
                primvarRefiner.InterpolateFaceUniform(lvl, srcFUnifMat, dstFUnifMat);
                srcFUnifMat = dstFUnifMat;
                face_offset += refiner->GetLevel(lvl).GetNumFaces();
            }
        }

    }

    auto lastLevel = refiner->GetLevel(level);
    const size_t nfaces = lastLevel.GetNumFaces();
    for (size_t i = 0; i < nfaces; i++) {
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
    matIdbuffer.clear();

}

void dhdm::MeshSubdivider::set_non_vert_data( dhdm::Mesh & targetMesh )
{
    targetMesh.vweights.clear();
    targetMesh.uv_layers.clear();
    targetMesh.faces.clear();

    if (uses_vgroups)
        targetMesh.vweights = vweightsbuffer;

    if (uses_uvs)
        targetMesh.uv_layers = uv_layers_buffers;

    targetMesh.faces = faces;
}


void dhdm::MeshSubdivider::subdivide_simple( dhdm::Mesh & targetMesh )
{
    if (level == 0) return;
    if (!is_simple_init) subdivide_simple_init();

    std::cout << fmt::format("Subdividing (simple subdivider) to level {}...\n", level);

    vertexStencils->UpdateValues( targetMesh.vertices.data(), &vbuffer[0] );
    /*
    if (uses_vgroups)
        vertexStencils->UpdateValues(targetMesh.vweights.data(), &vweightsbuffer[0]);
    if (uses_uvs)
        faceVaryingStencils->UpdateValues(targetMesh.uvs.data(), &uvbuffer[0]);
    */

    targetMesh.vertices.clear();
    targetMesh.vertices = vbuffer;

    set_non_vert_data(targetMesh);
}


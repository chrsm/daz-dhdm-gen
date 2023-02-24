#pragma once

#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/normal.hpp>
#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/far/stencilTable.h>
#include <opensubdiv/far/stencilTableFactory.h>

#include "tinyxml2.h"
#include "shared.hh"


namespace dhdm {

extern double gScale;

struct Vertex
{
    glm::dvec3 pos;

    // Interfaces expected by OSD.
    void Clear(void * = nullptr)
    {
        pos = {0.0, 0.0, 0.0};
    }

    void AddWithWeight(Vertex const & src, float weight)
    {
        pos += src.pos * (double) weight;
    }
};

typedef uint32_t VertexId;
typedef uint32_t UvId;
typedef uint32_t FaceId;

struct FaceVertex
{
    VertexId vertex;
    UvId uv;
};

struct Face
{
    std::vector<FaceVertex> vertices;
    short matId;
};

struct UV
{
    glm::dvec2 pos;

    // Interfaces expected by OSD.
    void Clear(void * = nullptr) {
        pos = {0.0, 0.0};
    }

    void AddWithWeight(UV const & src, float weight) {
        pos += src.pos * (double) weight;
    }
};


struct VertexWeights
{
    std::unordered_map<short, float> weights;

    // Interfaces expected by OSD.
    void Clear(void * = nullptr)
    {
        weights.clear();
    }

    void AddWithWeight(VertexWeights const & src, float weight)
    {
        for (auto it = src.weights.begin(); it != src.weights.end(); ++it) {
            const short srcVG = it->first;
            if ( weights.count(srcVG) > 0 )
                weights[srcVG] += it->second * weight;
            else
                weights[srcVG] = it->second * weight;
        }
    }
};


struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<Face> faces;

    bool uses_uvs = false;
    std::vector<std::vector<UV>> uv_layers;

    bool uses_materials = false;
    std::vector<std::string> materialNames;

    bool uses_vgroups = false;
    std::vector<std::string> vgroupsNames;
    std::vector<VertexWeights> vweights;

    bool subd_only_deltas = false;
    const Mesh * originalMesh = nullptr;


    static Mesh fromObj( const std::string & fp,
                         const bool load_uvs,
                         const bool load_materials,
                         const bool use_face_id_mat_id );

    FaceMap faceMapfromObj( const char * fp_obj );

    static Mesh fromDSF(const std::string & geoFile, const std::string & uvFile);

    static Mesh fromDae( const char * fp_dae,
                         const char * fp_obj,
                         const short load_uv_layers,
                         bool load_materials,
                         bool load_vgroups );

    void subdivide_nonvertex( const unsigned int level,
                              std::unique_ptr<OpenSubdiv::Far::TopologyRefiner> & refiner,
                              OpenSubdiv::Far::PrimvarRefiner & primvarRefiner );

    void subdivide_simple(const unsigned int level);

    void triangulate();

    void writeCollada(const std::string & fp, const std::string & name);

    void writeObj(const std::string & fp);

    void set_subd_only_deltas(const Mesh * originalMesh);

private:
    void insertVerticesPosSource(tinyxml2::XMLDocument & dae, tinyxml2::XMLElement* parent);
    void insertUVsSource( tinyxml2::XMLDocument & dae, tinyxml2::XMLElement* parent,
                          const std::vector<std::string> & uv_layers_names );
    void insertPolylist( tinyxml2::XMLDocument & dae, tinyxml2::XMLElement* parent,
                         const std::vector< std::vector<Face> > & materials_faces,
                         const std::vector<std::string> & uv_layers_names );
    void insertJointsSource(tinyxml2::XMLDocument & dae, tinyxml2::XMLElement* parent);
    void insertBindPosesSource(tinyxml2::XMLDocument & dae, tinyxml2::XMLElement* parent);
    void insertWeightsSource(tinyxml2::XMLDocument & dae, tinyxml2::XMLElement* parent);
    void insertVertexWeights(tinyxml2::XMLDocument & dae, tinyxml2::XMLElement* parent);
    void insertSceneNodes( tinyxml2::XMLDocument & dae, tinyxml2::XMLElement* parent,
                           const std::string & name, const short num_materials );
};


OpenSubdiv::Far::TopologyRefiner * createTopologyRefiner( const unsigned int level, const Mesh & baseMesh );


class MeshSubdivider
{
public:
    MeshSubdivider( const Mesh * baseMesh, const unsigned int level );

    void subdivide_simple( Mesh & targetMesh );

private:
    unsigned int level;
    bool uses_uvs;
    bool uses_vgroups;
    bool uses_materials;

    bool is_simple_init = false;
    bool is_dhdm_init = false;

    const Mesh * baseMesh = nullptr;

    std::unique_ptr<OpenSubdiv::Far::TopologyRefiner> refiner;

    std::vector<std::vector<UV>> uv_layers_buffers;
    std::vector<VertexWeights> vweightsbuffer;
    std::vector<short> matIdbuffer;
    std::vector<Face> faces;

    std::unique_ptr<const OpenSubdiv::Far::StencilTable> vertexStencils;
    // std::unique_ptr<const OpenSubdiv::Far::StencilTable> faceVaryingStencils;
    std::vector<Vertex> vbuffer;

    void subdivide_simple_init();
    void set_non_vert_data( Mesh & targetMesh );
};

}

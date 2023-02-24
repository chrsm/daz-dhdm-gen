#ifndef DHDM_CALC_H_INCLUDED
#define DHDM_CALC_H_INCLUDED
#include "mesh.hh"


class DhdmWriter
{
public:
    DhdmWriter( const dhdm::Mesh *base_mesh, const dhdm::Mesh *hd_mesh,
                const FilepathsInfo* fps_info );

    void calculateDhdm();
    void writeDhdm(const std::string filepath);

private:
    static constexpr uint32_t MAG1 = 0xd0d0d0d0;
    static constexpr uint32_t MAG2 = 0x3f800000;

    struct VertDisp
    {
        float x;
        uint8_t b1;
        uint8_t b2;
        uint8_t b3;
        uint8_t b4;
        float y;
        float z;
    };

    struct LevelDisps
    {
        uint32_t faceIdx;
        uint32_t vertices;
        std::vector<DhdmWriter::VertDisp> vert_disps;
    };

    struct LevelHeader
    {
        uint32_t nr_faces;
        uint32_t level;
        uint32_t nrDisplacements;
        uint32_t data_size;
        std::vector<DhdmWriter::LevelDisps> level_disps;
    };

    struct DhdmFileData
    {
        uint32_t magic1;
        uint32_t nr_levels;
        uint32_t magic2;
        uint32_t nr_levels2;

        std::vector<DhdmWriter::LevelHeader> levels_headers;
    };

    const dhdm::Mesh * base_mesh;
    const dhdm::Mesh * hd_mesh;
    const FilepathsInfo* fps_info;
    DhdmFileData dhdm_fd;

    void calc_inverse_dhdm_mats( std::vector< std::vector<glm::dmat3x3> > & mats,
                                 std::vector<int> & firstLevelSubFaceOffset );
};

#endif // DHDM_CALC_H_INCLUDED

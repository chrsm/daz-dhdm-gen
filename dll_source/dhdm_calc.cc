#include <iostream>
#include <fstream>
#include <fmt/format.h>
#include <unordered_map>
#include <set>

#include "dhdm_calc.hh"
#include "utils.hh"

using namespace OpenSubdiv;


DhdmWriter::DhdmWriter( const dhdm::Mesh *base_mesh, const dhdm::Mesh *hd_mesh,
                        const FilepathsInfo* fps_info ) :
    base_mesh(base_mesh), hd_mesh(hd_mesh), fps_info(fps_info)
{
}


void DhdmWriter::calc_inverse_dhdm_mats( std::vector< std::vector<glm::dmat3x3> > & mats,
                                         std::vector<int> & firstLevelSubFaceOffset )
{
    int subFaceOffset = 0;
    for (auto & face : base_mesh->faces) {
        const int num_face_verts = face.vertices.size();
        assert( num_face_verts >= 3 );

        firstLevelSubFaceOffset.push_back(subFaceOffset);
        subFaceOffset += num_face_verts;

        std::vector< glm::dvec3 > face_coords;
        for (int i = 0; i < num_face_verts; i++)
            face_coords.push_back( base_mesh->vertices[face.vertices[i].vertex].pos );

        /*
        glm::dvec3 x_axis = glm::normalize( face_coords[3] - face_coords[0] );
        glm::dvec3 z_axis = glm::normalize( face_coords[1] - face_coords[0] );
        glm::dvec3 y_axis = -glm::normalize( glm::cross(x_axis, z_axis) );
        */

        std::vector<glm::dmat3x3> face_mats;
        glm::dvec3 z_axis = glm::normalize( glm::cross( face_coords[1]-face_coords[0],
                                                        face_coords[num_face_verts-1]-face_coords[0] ) );
        glm::dvec3 x_axis;
        glm::dvec3 y_axis;

        for (int i = 0; i < num_face_verts; i++)
        {
            const int prev = (i > 0) ? (i - 1) : (num_face_verts-1);
            x_axis = glm::normalize( face_coords[ prev ] - face_coords[ i ] );
            y_axis = glm::normalize( glm::cross(z_axis, x_axis) );
            face_mats.push_back( glm::inverse(glm::dmat3x3(x_axis, z_axis, -y_axis)) );
        }

        mats.push_back(std::move(face_mats));
    }

}


void DhdmWriter::calculateDhdm()
{
    uint32_t level = relative_subd_level(*base_mesh, *hd_mesh);
    std::cout << fmt::format("Subdivision levels: {}.\n", level);
    if (level == 0)
        return;

    const bool do_translate = (fps_info != nullptr) && (fps_info->fps_count > 0);
    nlohmann::json vi_translate;
    if (do_translate)
    {
        if ( fps_info->fps_count < level )
            throw std::runtime_error( fmt::format("matching files with max level {} < subdivisions level {}",
                                              fps_info->fps_count, level ) );
        vi_translate = readJSON( fps_info->filepaths[level-1] );
    }

    std::cout << "Calculating dhdm...\n";

    dhdm_fd.magic1 = MAG1;
    dhdm_fd.magic2 = MAG2;
    dhdm_fd.nr_levels = level;
    dhdm_fd.nr_levels2 = level;

    std::vector< std::vector<glm::dmat3x3> > mats;
    std::vector<int> firstLevelSubFaceOffset;
    calc_inverse_dhdm_mats(mats, firstLevelSubFaceOffset);

    /* ----------- subd ------------ */
    std::cout << fmt::format("Subdividing to level {}...\n", level);
    dhdm::Mesh base_mesh_sd = *base_mesh;
    base_mesh_sd.uv_layers.clear();
    base_mesh_sd.uses_uvs = false;

    std::unique_ptr<Far::TopologyRefiner> refiner( dhdm::createTopologyRefiner( level, base_mesh_sd ) );
    Far::PrimvarRefiner primvarRefiner(*refiner);
    auto lastLevel = refiner->GetLevel(level);
    const int last_level_faces = lastLevel.GetNumFaces();
    const int last_level_verts = lastLevel.GetNumVertices();

    /* material faces */
    std::vector<uint32_t> matIdbuffer;
    {
        matIdbuffer.resize( refiner->GetNumFacesTotal() );
        int face_offset = base_mesh_sd.faces.size();
        for ( size_t i=0; i < face_offset; i++ )
            matIdbuffer[i] = base_mesh_sd.faces[i].matId;
        uint32_t * srcFUnifMat = matIdbuffer.data();
        for (unsigned int lvl = 1; lvl <= level; ++lvl)
        {
            auto dstFUnifMat = (matIdbuffer.data() + face_offset);
            primvarRefiner.InterpolateFaceUniform(lvl, srcFUnifMat, dstFUnifMat);
            srcFUnifMat = dstFUnifMat;
            face_offset += refiner->GetLevel(lvl).GetNumFaces();
        }
    }

    /* vertices */
    std::vector<dhdm::Vertex> vbuffer_pv( refiner->GetNumVerticesTotal() - base_mesh_sd.vertices.size() );
    dhdm::Vertex * srcVerts = base_mesh_sd.vertices.data();
    size_t vert_offset = 0;
    size_t face_offset = base_mesh_sd.faces.size();
    size_t prev_level_verts = base_mesh_sd.vertices.size();
    // std::set<uint32_t> inserted_verts;

    for (unsigned int lvl = 1; lvl <= level; ++lvl)
    {
        std::cout << "Calculating level " << lvl << "...";
        auto dstVerts = vbuffer_pv.data() + vert_offset;
        primvarRefiner.Interpolate(lvl, srcVerts, dstVerts);

        auto this_level = refiner->GetLevel(lvl);
        const size_t this_level_verts = this_level.GetNumVertices();
        const size_t this_level_faces = this_level.GetNumFaces();

        LevelHeader lh;
        lh.nr_faces = base_mesh_sd.faces.size();
        lh.level = lvl;
        lh.nrDisplacements = 0;
        lh.data_size = 0;

        const uint8_t lvl_number = (lvl + 1) * 16;
        const uint32_t subFaceOffsetFactor = ( 1 << ( 2 * (lvl-1) ) );
        std::unordered_map<uint32_t, uint32_t> face_disps_map;
        std::set<uint32_t> inserted_verts;

        for (int i = 0; i < this_level_faces; i++)
        {
            const uint32_t base_face_idx = matIdbuffer[ face_offset + i ];
            const uint32_t subface_idx = (uint32_t) (i - firstLevelSubFaceOffset[base_face_idx] * subFaceOffsetFactor);
            // auto currLevel_faceIdx = firstLevelSubFaceOffset[displ_faceIdx] * subFaceOffsetFactor + displ.subfaceIdx;

            const auto fverts = this_level.GetFaceVertices(i);
            //const auto fvuvs = this_level.GetFaceFVarValues(i, 0);
            assert(fverts.size() <= 4);

            for (int j = 0; j < fverts.size(); j++)
            {
                const uint32_t fvert_idx = j;
                const uint32_t vert_idx = (uint32_t) fverts[j];
                // const uint32_t uv_idx = (uint32_t) fvuvs[j];

                //if (vert_idx < prev_level_verts)
                //    continue;
                //if (lvl == 1)
                //    continue;
                if (inserted_verts.count(vert_idx) > 0)
                    continue;

                const glm::dvec3 & vert = vbuffer_pv[vert_offset + vert_idx].pos;
                glm::dvec3 delta;
                if (do_translate)
                {
                    const std::string vert_idx_str = std::to_string(vert_idx);
                    if (!vi_translate.contains(vert_idx_str))
                    {
                        throw std::runtime_error( fmt::format("Vertex index {} not found in json.\n",
                                                  vert_idx_str) );
                    }
                    const unsigned int vi = vi_translate[vert_idx_str];
                    if (vi >= hd_mesh->vertices.size())
                    {
                        throw std::runtime_error( fmt::format("Vertex index {} not found in hd_mesh.\n",
                                                  vi) );
                    }
                    const glm::dvec3 & hd_vert = hd_mesh->vertices[vi].pos;
                    delta = hd_vert - vert;
                }
                else
                {
                    const glm::dvec3 & hd_vert = hd_mesh->vertices[vert_idx].pos;
                    delta = hd_vert - vert;
                }

                const double minimum_disp = 1e-2;
                if (glm::length(delta) > minimum_disp)
                {
                    const uint32_t submat_idx = uint32_t( subface_idx / subFaceOffsetFactor );
                    /*
                    std::cout << "lvl = " << lvl << std::endl;
                    std::cout << "subFaceOffsetFactor = " << subFaceOffsetFactor << std::endl;
                    std::cout << "this_level_faces = " << this_level_faces << std::endl;
                    std::cout << "i = " << i << std::endl;
                    std::cout << "base_face_idx = " << base_face_idx << std::endl;
                    std::cout << "subface_idx = " << subface_idx << std::endl;
                    std::cout << "submat_idx = " << submat_idx << std::endl;
                    */
                    const glm::dvec3 delta_tan = mats[base_face_idx][submat_idx] * delta;

                    DhdmWriter::VertDisp vert_disp;
                    vert_disp.x = delta_tan[0];
                    vert_disp.y = delta_tan[1];
                    vert_disp.z = delta_tan[2];

                    vert_disp.b1 = 0;
                    vert_disp.b2 = 0;
                    vert_disp.b3 = 0;
                    vert_disp.b4 = 0;

                    lh.nrDisplacements++;
                    inserted_verts.insert(vert_idx);

                    if (lvl < 4)
                    {
                        vert_disp.b2 = lvl_number;
                        const unsigned short shift = 8 - (lvl << 1);
                        vert_disp.b1 = subface_idx << shift;
                        vert_disp.b1 = vert_disp.b1 | ( fvert_idx << (shift - 2) );
                    }
                    else
                    {
                        vert_disp.b4 = lvl_number;
                        const unsigned short shift = 16 - (lvl << 1);
                        uint16_t tmp = subface_idx << shift;
                        tmp = tmp | ( fvert_idx << (shift - 2) );
                        vert_disp.b3 = (uint8_t)(tmp >> 8);
                        vert_disp.b2 = (uint8_t)(tmp & 0x00ff);
                    }

                    if (face_disps_map.count(base_face_idx) > 0)
                    {
                        DhdmWriter::LevelDisps & lvl_face_disps = lh.level_disps[ face_disps_map[base_face_idx] ];
                        lvl_face_disps.vertices++;
                        lvl_face_disps.vert_disps.push_back(std::move(vert_disp));
                    }
                    else
                    {
                        DhdmWriter::LevelDisps lvl_face_disps;
                        lvl_face_disps.faceIdx = base_face_idx;
                        lvl_face_disps.vertices = 1;
                        lvl_face_disps.vert_disps.push_back(std::move(vert_disp));

                        face_disps_map[base_face_idx] = lh.level_disps.size();
                        lh.level_disps.push_back(std::move(lvl_face_disps));
                    }

                    vbuffer_pv[vert_offset + vert_idx].pos += delta;
                }
            }
        }

        uint32_t tot_lvl_size = 0;
        for (size_t i = 0; i < lh.level_disps.size(); i++)
        {
            tot_lvl_size += sizeof(uint32_t) * 2;

            const LevelDisps & lvl_face_disps = lh.level_disps[i];
            for (size_t j = 0; j < lvl_face_disps.vert_disps.size(); j++)
            {
                if (lh.level < 4)
                    tot_lvl_size += sizeof(float) * 3 + sizeof(uint8_t) * 2;
                else
                    tot_lvl_size += sizeof(float) * 3 + sizeof(uint8_t) * 4;
            }
        }
        lh.data_size = tot_lvl_size;

        std::cout << fmt::format("  displacements in level {}: {}.\n", lvl, lh.nrDisplacements);
        std::cout << fmt::format("  nr_faces in level {}: {}.\n", lvl, lh.nr_faces);
        std::cout << fmt::format("  base faces with displacements in level {}: {}.\n\n", lvl, face_disps_map.size());

        dhdm_fd.levels_headers.push_back(std::move(lh));
        srcVerts = dstVerts;
        vert_offset += this_level_verts;
        face_offset += this_level_faces;
        prev_level_verts = this_level_verts;
    }

    std::cout << "Finished calculating dhdm." << std::endl;
}


void DhdmWriter::writeDhdm(const std::string filepath)
{
    std::cout << "Writing \"" << filepath << "\"...\n";
    std::ofstream out_file;
    out_file.open( filepath, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc );

    out_file.write( (char*) &dhdm_fd, sizeof(uint32_t) * 4 );

    for (size_t i=0; i < dhdm_fd.levels_headers.size(); i++)
    {
        const LevelHeader & curr_lvl_h = dhdm_fd.levels_headers[i];
        out_file.write( (char*) &curr_lvl_h, sizeof(uint32_t) * 4 );

        uint32_t lvl_data_size = 0;

        for (size_t j=0; j < curr_lvl_h.level_disps.size(); j++)
        {
            const LevelDisps & curr_fdisp = curr_lvl_h.level_disps[j];

            lvl_data_size += sizeof(uint32_t) * 2;
            out_file.write( (char*) &curr_fdisp, sizeof(uint32_t) * 2 );

            assert(curr_fdisp.vertices == curr_fdisp.vert_disps.size());
            for (size_t k=0; k < curr_fdisp.vert_disps.size(); k++)
            {
                const VertDisp & curr_vdisp = curr_fdisp.vert_disps[k];

                if (curr_lvl_h.level < 4)
                {
                    lvl_data_size += sizeof(float) * 3 + sizeof(uint8_t) * 2;

                    out_file.write( (char*) &curr_vdisp.x, sizeof(float) );
                    out_file.write( (char*) &curr_vdisp.b1, sizeof(uint8_t) );
                    out_file.write( (char*) &curr_vdisp.b2, sizeof(uint8_t) );
                    out_file.write( (char*) &curr_vdisp.y, sizeof(float) );
                    out_file.write( (char*) &curr_vdisp.z, sizeof(float) );
                }
                else
                {
                    lvl_data_size += sizeof(float) * 3 + sizeof(uint8_t) * 4;

                    out_file.write( (char*) &curr_vdisp.x, sizeof(float) );
                    out_file.write( (char*) &curr_vdisp.b1, sizeof(uint8_t) );
                    out_file.write( (char*) &curr_vdisp.b2, sizeof(uint8_t) );
                    out_file.write( (char*) &curr_vdisp.b3, sizeof(uint8_t) );
                    out_file.write( (char*) &curr_vdisp.b4, sizeof(uint8_t) );
                    out_file.write( (char*) &curr_vdisp.y, sizeof(float) );
                    out_file.write( (char*) &curr_vdisp.z, sizeof(float) );
                }
            }
        }

        assert( lvl_data_size == curr_lvl_h.data_size );
    }

    out_file.close();
    std::cout << "Done writing .dhdm file.\n";
}


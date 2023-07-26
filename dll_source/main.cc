#include <iostream>
#include <fstream>
#include <future>
#include <fmt/format.h>

#include "main.hh"
#include "utils.hh"
#include "dhdm_calc.hh"


DLL_EXPORT int generate_hd_mesh( const MeshInfo* mesh_info,
                                 const char* output_dirpath,
                                 const char* output_filename )
{
    try{
        dhdm::gScale = 1; // mesh_info->gScale;
        const std::string fp_obj = std::string(mesh_info->base_exportedf) + ".obj";
        dhdm::Mesh loadedBaseMesh = dhdm::Mesh::fromObj( fp_obj, false, false, false );

        loadedBaseMesh.subdivide_simple( mesh_info->hd_level );

        const std::string filename(output_filename);
        const std::string filepath( std::string(output_dirpath) + "/" + filename + ".obj" );
        loadedBaseMesh.writeObj(filepath);
        return 0;

    } catch (std::exception & e) {
        std::cout << "-Error in DLL: " << e.what() << std::endl;
        return -1;
    }
}


DLL_EXPORT int generate_dhdm_file( const MeshInfo* mesh_info,
                                   const FilepathsInfo* fps_info,
                                   const char* output_dirpath,
                                   const char* output_filename )
{
    try{
        dhdm::gScale = mesh_info->gScale;
        const std::string fp_base = std::string(mesh_info->base_exportedf) + ".obj";
        const std::string fp_hd_edit = std::string(mesh_info->base_exportedf) + "_hd_edit.obj";
        dhdm::Mesh baseMesh = dhdm::Mesh::fromObj( fp_base, false, false, true );
        dhdm::Mesh editedhdMesh = dhdm::Mesh::fromObj( fp_hd_edit, false, false, true );

        std::set<uint32_t> edited_vis;
        {
            const std::string fp_hd_no_edit = std::string(mesh_info->base_exportedf) + "_hd_no_edit.obj";
            dhdm::Mesh noeditedhdMesh = dhdm::Mesh::fromObj( fp_hd_no_edit, false, false, true );
            if ( mesh_info->is_subd_daz )
            {
                noeditedhdMesh.subdivide_simple( mesh_info->hd_level );
                fps_info = nullptr;
            }
            edited_vis = get_hd_disp_mask(noeditedhdMesh, editedhdMesh);
        }
        std::cout << fmt::format("Number of vertices detected as edited: {}.\n", edited_vis.size());

        DhdmWriter dhdm_writer(&baseMesh, &editedhdMesh, fps_info, &edited_vis);
        dhdm_writer.calculateDhdm();

        const std::string dhdm_filepath( std::string(output_dirpath) + "/" + std::string(output_filename) + ".dhdm" );
        dhdm_writer.writeDhdm(dhdm_filepath);

        return 0;
    } catch (std::exception & e) {
        std::cout << "-Error in DLL: " << e.what() << std::endl;
        return -1;
    }
}

//-------------------------------------------------------

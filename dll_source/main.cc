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

        DhdmWriter dhdm_writer(&baseMesh, &editedhdMesh, fps_info);
        dhdm_writer.calculateDhdm();

        const std::string dhdm_filename(output_filename);
        const std::string dhdm_filepath( std::string(output_dirpath) + "/" + dhdm_filename + ".dhdm" );
        dhdm_writer.writeDhdm(dhdm_filepath);

        return 0;
    } catch (std::exception & e) {
        std::cout << "-Error in DLL: " << e.what() << std::endl;
        return -1;
    }
}

//-------------------------------------------------------

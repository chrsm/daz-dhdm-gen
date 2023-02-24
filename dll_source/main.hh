#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include "shared.hh"

#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT __declspec(dllimport)
#endif

extern "C" {

    DLL_EXPORT int generate_hd_mesh( const MeshInfo* mesh_info,
                                     const char* output_dirpath,
                                     const char* output_filename );

    DLL_EXPORT int generate_dhdm_file( const MeshInfo* mesh_info,
                                       const FilepathsInfo* fps_info,
                                       const char* output_dirpath,
                                       const char* output_filename );


    //-------------------------------------------------------
    /*
    DLL_EXPORT BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
        switch (fdwReason) {
            case DLL_PROCESS_ATTACH:
                // attach to process
                // return FALSE to fail DLL load
                break;

            case DLL_PROCESS_DETACH:
                // detach from process
                break;

            case DLL_THREAD_ATTACH:
                // attach to thread
                break;

            case DLL_THREAD_DETACH:
                // detach from thread
                break;
        }
        return TRUE; // succesful
    }
    */
}
#endif // __MAIN_H__

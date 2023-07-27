import os, ctypes, concurrent.futures
from . import utils


def str_2_char_p(string):
    if string is None:
        return ctypes.c_char_p(None)
    return ctypes.c_char_p(bytes(string, encoding="utf-8"))

def str_2_char(string):
    return ctypes.c_char(bytes(string[0], encoding="utf-8"))


class FilepathsInfo(ctypes.Structure):
    _fields_ = [ ( "filepaths", ctypes.POINTER(ctypes.c_char_p) ),
                 ( "fps_count", ctypes.c_ushort ) ]

    def __init__(self, filepaths_list):
        fps_count = len(filepaths_list)
        filepaths = (ctypes.c_char_p * fps_count)()
        for i, fp in enumerate(filepaths_list):
            filepaths[i] = str_2_char_p(fp)

        self.fps_count = ctypes.c_ushort( fps_count )
        self.filepaths = ctypes.cast( filepaths, ctypes.POINTER(ctypes.c_char_p) )

class MeshInfo(ctypes.Structure):
    _fields_ = [ ("gScale", ctypes.c_float ),
                 ("base_exportedf", ctypes.c_char_p),
                 ("hd_level", ctypes.c_ushort),
                 ("load_uv_layers", ctypes.c_short) ]

    def __init__( self, gScale, base_exportedf, load_uv_layers=-1, hd_level=0 ):
        self.gScale = ctypes.c_float(gScale)
        self.base_exportedf = str_2_char_p(base_exportedf)
        self.hd_level = ctypes.c_ushort(hd_level)
        self.load_uv_layers = ctypes.c_short(load_uv_layers)


class DHDM_DLL_Wrapper:
    dll_path = os.path.join(os.path.dirname(__file__), "dll_dir", "dhdm_gen_dll.dll")

    def __init__(self):
        if not os.path.isfile(self.dll_path):
            raise RuntimeError("File \"{0}\" not found.".format(self.dll_path))
        try:
            self.dll = ctypes.cdll.LoadLibrary(self.dll_path)
        except OSError as e:
            print("Failed to load \"{0}\".".format(self.dll_path))
            raise e

    def generate_hd_mesh( self, gScale, base_exportedf,
                                hd_level, outputDirpath,
                                outputFilename ):

        mesh_info = MeshInfo( gScale, base_exportedf, hd_level=hd_level )

        r = self.dll.generate_hd_mesh( ctypes.byref(mesh_info),
                                       str_2_char_p(outputDirpath),
                                       str_2_char_p(outputFilename) )

        if r is None or r != 0:
            raise RuntimeError("Function \"{0}\" in \"{1}\" failed.".format("generate_hd_mesh()", self.dll_path))
        return r


    def generate_dhdm_file( self,
                            gScale, base_exportedf, hd_level,
                            outputDirpath, outputFilename,
                            filepaths_list ):

        mesh_info = MeshInfo( gScale, base_exportedf, hd_level=hd_level )
        fps_info = FilepathsInfo( filepaths_list )

        r = self.dll.generate_dhdm_file( ctypes.byref(mesh_info),
                                         ctypes.byref(fps_info),
                                         str_2_char_p(outputDirpath),
                                         str_2_char_p(outputFilename) )

        if r is None or r != 0:
            raise RuntimeError("Function \"{0}\" in \"{1}\" failed.".format("generate_dhdm_file()", self.dll_path))
        return r


def call_dll_function(func_name, *args ):
    w = DHDM_DLL_Wrapper()
    func = getattr(w, func_name)
    print("\n---- Start of DLL ----\n")
    try:
        r = func(*args)
        del w
        print("\n---- End of DLL ----\n")
        return r
    except Exception as e:
        del w
        print("\n---- ERROR in DLL (read console output).\n")
        raise e

def execute_in_new_thread( func_name, *args ):
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as executor:
        future = executor.submit( call_dll_function, func_name, *args )
        r = future.result()
    return r

#ifndef SHARED_H_INCLUDED
#define SHARED_H_INCLUDED

#include <tuple>
#include <unordered_map>

extern "C" {

struct FilepathsInfo
{
    char** filepaths;
    short fps_count;
};

struct MeshInfo
{
    float gScale;
    char* base_exportedf;
    unsigned short hd_level;
    short load_uv_layers;
};

}   // extern C


namespace hash_tuple
{
    template <typename T>
    struct hash_f
    {
        size_t operator()(T const& v) const
        {
            return std::hash<T>()(v);
        }
    };

    namespace
    {
        template <class T>
        inline void hash_combine(size_t& seed, T const& v) {
            seed ^= hash_tuple::hash_f<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        }

        template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
        struct HashValueImpl
        {
          static void apply(size_t& seed, Tuple const& tup)
          {
            HashValueImpl<Tuple, Index-1>::apply(seed, tup);
            hash_combine(seed, std::get<Index>(tup));
          }
        };

        template <class Tuple>
        struct HashValueImpl<Tuple, 0>
        {
          static void apply(size_t& seed, Tuple const& tup)
          {
            hash_combine(seed, std::get<0>(tup));
          }
        };
    }

    template <typename ... Ts>
    struct hash_f<std::tuple<Ts...>>
    {
        size_t operator()(std::tuple<Ts...> const& tup) const
        {
            size_t seed = 0;
            HashValueImpl<std::tuple<Ts...> >::apply(seed, tup);
            return seed;
        }
    };
}

using FaceTuple = std::tuple<unsigned int, unsigned int, unsigned int, unsigned int>;
using FaceMap = std::unordered_map< FaceTuple, unsigned int, hash_tuple::hash_f<FaceTuple> >;

#endif // SHARED_H_INCLUDED

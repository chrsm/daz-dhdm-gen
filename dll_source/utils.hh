#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <vector>
#include <string>
#include <tuple>
#include <unordered_map>
#include <set>
#include <nlohmann/json.hpp>

#include "shared.hh"
#include "mesh.hh"


nlohmann::json readJSON(const std::string & fp);

void print_vertex(std::vector<dhdm::Vertex> & vertices, const size_t i);


template <typename T>
class NumberReader
{
public:

    explicit NumberReader( const char* p )
    {
        p0 = p;
        p1 = nullptr;
        ended = false;

        val = read_func();
        if (errno != 0 || p1 == p0)
        {
            ended = true;
            p0 = nullptr;
            p1 = nullptr;
        }
        else
        {
            p0 = p1;
        }
    }

    T read_next()
    {
        if (ended)
            throw std::runtime_error("NumberReader: reached end of reading");

        const T r = val;
        val = read_func();

        if (errno != 0 || p1 == p0)
        {
            ended = true;
            p0 = nullptr;
            p1 = nullptr;
        }
        else
        {
            p0 = p1;
        }
        return r;
    }

    bool has_next()
    {
        return !ended;
    }

private:
    T read_func();

    const char* p0;
    char * p1;
    bool ended;
    T val;
};


template<> inline
double NumberReader<double>::read_func()
{
    return strtod( p0, &p1 );
}

template<> inline
unsigned long NumberReader<unsigned long>::read_func()
{
    return strtoul( p0, &p1, 10 );
}


std::vector<std::string> split( const std::string & p, const std::string & d );

bool endswith( const std::string & p, const std::string & suf );

uint32_t relative_subd_level(dhdm::Mesh base_mesh, dhdm::Mesh hd_mesh);

#endif // UTILS_H_INCLUDED

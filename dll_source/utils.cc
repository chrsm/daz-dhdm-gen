#include <fstream>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <fmt/format.h>

#include "utils.hh"


nlohmann::json readJSON(const std::string & fp)
{
    auto fs = std::fstream(fp, std::fstream::in | std::fstream::binary);
    if (!fs)
        throw std::runtime_error("Can't open file");

    unsigned char magic[2];
    fs.read((char *) magic, sizeof(magic));
    fs.seekg(0, fs.beg);

    nlohmann::json json;

    if (magic[0] == 0x1f && magic[1] == 0x8b) {
        boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
        in.push(boost::iostreams::gzip_decompressor());
        in.push(fs);
        std::istream str(&in);
        str >> json;
    } else {
        fs >> json;
    }

    fs.close();
    return json;
}


void print_vertex(std::vector<dhdm::Vertex> & vertices, const size_t i)
{
    std::cout << "  Vertex " << i << ": " << "(";
    std::cout << vertices[i].pos[0] << ", ";
    std::cout << vertices[i].pos[1] << ", ";
    std::cout << vertices[i].pos[2] << ")\n";
}


std::vector<std::string> split( const std::string & p, const std::string & d )
{
    std::vector<std::string> r;
    size_t i = 0;
    for (size_t j = p.find(d); j != std::string::npos; j = p.find(d, i))
    {
        r.emplace_back( p.begin() + i, p.begin() + j ) ;
        i = j + d.size();
    }
    if (i != p.size())
        r.emplace_back( p.begin() + i, p.end() );
    return r;
}

bool endswith ( const std::string & p, const std::string & suf ) {
    if ( suf.size() > p.size() )
        return false;
    return p.compare( p.size() - suf.size(), suf.size(), suf ) == 0;
}


uint32_t relative_subd_level(dhdm::Mesh base_mesh, dhdm::Mesh hd_mesh)
{
    const unsigned int n_faces_b = base_mesh.faces.size();
    const unsigned int n_faces_h = hd_mesh.faces.size();
    const double r = double(n_faces_h) / n_faces_b;

    double n = std::round( std::log(r) / std::log(4) );
    assert( n >= 0 );
    return (uint32_t) n;
}


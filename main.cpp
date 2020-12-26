#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "lib/common/error.hpp"
#include "lib/common/filesystem.hpp"
#include "lib/common/system.hpp"
#include "lib/common/timer.hpp"
#include "lib/cxxopts.hpp"
#include "zlib.h"

#include "edge.hpp"
#include "graphalytics_algorithms.hpp"
#include "graphalytics_reader.hpp"

using namespace common;
using namespace std;

bool g_compress_output = false; // whether to compress (.zip) the output edges and vertices
string g_path_input; // path to the input graph, in the Graphalytics format
string g_path_output; // path to the output graph
bool g_sorted_order_vertices = false; // whether to remap the vertices following the same sorted order of the input

// logging
#define LOG(msg) { std::scoped_lock xlock_log(g_mutex_log); std::cout << msg << std::endl; }
mutex g_mutex_log;

// function prototypes
static void parse_command_line_arguments(int argc, char* argv[]);
static pair<uint64_t, vector<WeightedEdge>> parse_input(GraphalyticsReader& reader, GraphalyticsAlgorithms& algorithms);
static void sort_edges(vector<WeightedEdge>& edges);
static void save_properties(GraphalyticsReader& reader, GraphalyticsAlgorithms& algorithms, const string& path_prefix);
static void save_vertices(uint64_t num_vertices, const string& path_output);
static void save_edges(vector<WeightedEdge>& edges, const string& path_output, bool is_weighted);
static string get_current_datetime();

int main(int argc, char* argv[]) {
    Timer timer; timer.start();

    try {
        parse_command_line_arguments(argc, argv);

        // read the input graph
        GraphalyticsReader reader(g_path_input);
        GraphalyticsAlgorithms algorithms(reader);
        auto input = parse_input(reader, algorithms);
        uint64_t num_vertices = input.first;
        sort_edges(input.second);

        // remove the suffix ".properties" from the end of the file name
        smatch matches;
        regex_match(g_path_output, matches, regex{"^(.+?)(\\.properties)?$"});
        string prefix = matches[1];

        // store the new graph
        save_properties(reader, algorithms, prefix);
        string path_vertices = prefix + (g_compress_output ? ".vz" : ".v");
        save_vertices(num_vertices, path_vertices);
        string path_edges = prefix + (g_compress_output ? ".ez" : ".e");
        save_edges(input.second, path_edges, reader.is_weighted());

    } catch (common::Error& e){
        cerr << e << endl;
        cerr << "Type `" << argv[0] << " --help' to check how to run the program\n";
        cerr << "Program terminated" << endl;
        return 1;
    }

    cout << "\nDone. Whole completion time: " << timer << "\n";

    return 0;
}

static pair<uint64_t, vector<WeightedEdge>> parse_input(GraphalyticsReader& reader, GraphalyticsAlgorithms& algorithms){
    unordered_map<uint64_t, uint64_t> vertices;
    vertices.reserve( stoull(reader.get_property("meta.vertices")));
    using P = decltype(vertices)::value_type;
    uint64_t next_vertex_id = 0;

    Timer timer; timer.start();
    if(g_sorted_order_vertices){ // respect the same sorted order of the vertices appearing in the input graph
        LOG("Reading the input vertices ...");

        uint64_t vertex_id = 0;
        while(reader.read_vertex(vertex_id)){
            vertices[vertex_id] = next_vertex_id++;
        }

        LOG("Input vertices parsed in " << timer);
        assert(vertices.size() == stoull(reader.get_property("meta.vertices")) && "Cardinality mismatch");
    }

    LOG("Reading the input edges ...");
    timer.start();

    pair<uint64_t, vector<WeightedEdge>> result;
    result.second.reserve(stoull(reader.get_property("meta.edges")));
    WeightedEdge edge;
    while(reader.read_edge(edge.m_source, edge.m_destination, edge.m_weight)){

        auto v1 = vertices.insert(P{edge.m_source, next_vertex_id});
        if(v1.second){ next_vertex_id++; } // new vertex
        edge.m_source = v1.first->second;

        auto v2 = vertices.insert(P{edge.m_destination, next_vertex_id});
        if(v2.second){ next_vertex_id++; } // new vertex
        edge.m_destination = v2.first->second;

        assert(edge.m_source != edge.m_destination && "Edge with the same source & destination is not allowed");
        if(!reader.is_directed() && edge.m_source > edge.m_destination) std::swap(edge.m_source, edge.m_destination); // src < dst

        result.second.push_back(edge);
    }

    result.first = next_vertex_id; // number of edges created

    // Source for the BFS algorithm
    if(algorithms.bfs.m_enabled){
        assert(vertices.count(algorithms.bfs.m_source_vertex) > 0 && "The vertex does not exist");
        algorithms.bfs.m_source_vertex = vertices[ algorithms.bfs.m_source_vertex ];
    }

    // Source for the SSSP algorithm
    if(algorithms.sssp.m_enabled){
        assert(vertices.count(algorithms.sssp.m_source_vertex) > 0 && "The vertex does not exist");
        algorithms.sssp.m_source_vertex = vertices [ algorithms.sssp.m_source_vertex ];
    }

    timer.stop();
    LOG("Input edges parsed in " << timer);

    return result;
}

static void sort_edges(vector<WeightedEdge>& edges){
    LOG("Sorting the list of edges ...");
    Timer timer; timer.start();

    std::sort(edges.data(), edges.data() + edges.size(), [](const WeightedEdge& e1, const WeightedEdge& e2){
        return (e1.m_source < e2.m_source) || (e1.m_source == e2.m_source && e1.m_destination < e2.m_destination);
    });

    timer.stop();
    LOG("Edges sorted in " << timer);
}

static void save_properties(GraphalyticsReader& reader, GraphalyticsAlgorithms& algorithms, const string& path_prefix){
    string path_output = path_prefix + ".properties";
    LOG("Saving the property file " << path_output << " ...");
    Timer timer; timer.start();

    fstream out {path_output , ios::out };
    if(!out.good()) ERROR("Cannot create the file `" << path_output << "'");
    out << "# Created by vtxremap, on " << get_current_datetime() << "\n\n";

//    string basedir = common::filesystem::directory(path_prefix);
    string basename = common::filesystem::filename(path_prefix);

    out << "# Filenames of graph on local filesystem\n";
    out << "graph." << basename << ".vertex-file = " << basename << (g_compress_output ? ".vz" : ".v") << "\n";
    out << "graph." << basename << ".edge-file = " << basename << (g_compress_output ? ".ez" : ".e") << "\n\n";

    out << "# Graph metadata for reporting purposes\n";
    out << "graph." << basename << ".meta.vertices = " << reader.get_property("meta.vertices") << "\n";
    out << "graph." << basename << ".meta.edges = " << reader.get_property("meta.edges") << "\n";
    out << "graph." << basename << ".meta.hostname = " << common::hostname() << "\n";
    out << "graph." << basename << ".meta.stable-map = " << boolalpha << g_sorted_order_vertices << "\n";
    out << "graph." << basename << ".meta.input-graph = " << common::filesystem::filename(g_path_input) << "\n\n";

    out << "# Properties describing the graph format\n";
    if(g_compress_output){ out << "graph." << basename << ".compression = zlib\n"; }
    out << "graph." << basename << ".directed = " << (reader.is_directed() ? "true" : "false") << "\n\n";

    if(reader.is_weighted()){
        out << "# Description of graph properties\n";
        out << "graph." << basename << ".edge-properties.names = weight\n";
        out << "graph." << basename << ".edge-properties.types = real\n\n";
    }

    out << "# List of supported algorithms on the graph\n";
    out << "graph." << basename << ".algorithms = " << reader.get_property("algorithms") << "\n\n";

    out << "\n";
    out << "#\n";
    out << "# Per-algorithm properties describing the input parameters to each algorithm\n";
    out << "#\n\n";

    if(algorithms.bfs.m_enabled) {
        out << "# Parameters for BFS\n";
        out << "graph." << basename << ".bfs.source-vertex = " << algorithms.bfs.m_source_vertex << "\n\n";
    }

    if(algorithms.cdlp.m_enabled){
        out << "# Parameters for CDLP\n";
        out << "graph." << basename << ".cdlp.max-iterations = " << algorithms.cdlp.m_max_iterations << "\n\n";
    }
    if(algorithms.lcc.m_enabled) {
        out << "# No parameters for LCC\n\n";
    }

    if(algorithms.pagerank.m_enabled){
        out << "# Parameters for PR\n";
        out << "graph." << basename << ".pr.damping-factor = " << algorithms.pagerank.m_damping_factor << "\n";
        out << "graph." << basename << ".pr.num-iterations = " << algorithms.pagerank.m_num_iterations << "\n\n";
    }

    if(algorithms.sssp.m_enabled){
        out << "# Parameters for SSSP\n";
        out << "graph." << basename << ".sssp.weight-property = weight\n";
        out << "graph." << basename << ".sssp.source-vertex = " << algorithms.sssp.m_source_vertex << "\n\n";
    }

    if(algorithms.wcc.m_enabled){
        out << "# No parameters for WCC\n";
    }

    out.close();

    timer.stop();
    LOG("Property file saved in " << timer);
}

static void save_vertices(uint64_t num_vertices, const string& path_output){
    LOG("Saving the vertex file " << path_output << " ...");
    Timer timer; timer.start();

    fstream out { path_output , ios::out | ios::binary };
    if(!out.good()) ERROR("Cannot create the file " << path_output);

    if(g_compress_output){ // compressed output
        constexpr uint64_t buffer_sz = (1 << 20); // * sizeof(uint64_t)
        unique_ptr<uint64_t[]> ptr_input_buffer { new uint64_t[buffer_sz] };
        uint64_t* input_buffer = ptr_input_buffer.get();
        unique_ptr<uint64_t[]> ptr_output_buffer { new uint64_t[buffer_sz] };
        unsigned char* output_buffer = reinterpret_cast<unsigned char*>(ptr_output_buffer.get());
        uint64_t next_vertex_id = 0;

        z_stream stream; int rc (0);
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = 0;
        stream.avail_out = 0;
        rc = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
        if(rc != Z_OK) ERROR("Cannot initialise the zlib stream");

        do {
            // input, the data that has to be compressed
            if(stream.avail_in == 0){
                uint64_t chunk_sz = min(num_vertices - next_vertex_id, buffer_sz);
                for(uint64_t i = 0; i < chunk_sz; i++){
                    input_buffer[i] = next_vertex_id +i;
                }
                next_vertex_id += chunk_sz;

                stream.next_in = reinterpret_cast<decltype(stream.next_in)>(input_buffer);
                stream.avail_in = chunk_sz * sizeof(uint64_t);
            }

            // output, the data compressed by zlib
            stream.avail_out = buffer_sz * sizeof(uint64_t);
            stream.next_out = output_buffer;

            // invoke zlib
            rc = deflate(&stream, (next_vertex_id >= num_vertices) ? Z_FINISH : Z_NO_FLUSH);
            if(rc != Z_OK && rc != Z_STREAM_END) ERROR("Compression error");

            // flush to disk
            uint64_t bytes_compressed = buffer_sz * sizeof(uint64_t) - stream.avail_out;
            out.write((char*) output_buffer, bytes_compressed);
        } while(stream.avail_in > 0 || next_vertex_id < num_vertices);

        rc = deflateEnd(&stream);
        if(rc != Z_OK) ERROR("Cannot close the zlib stream");

    } else { // plain output
        for(uint64_t i = 0; i < num_vertices; i++){
            out << i << "\n";
        }
    }

    out.close();
    timer.stop();
    LOG("Vertex file saved in " << timer);
}

static void save_edges(vector<WeightedEdge>& edges, const string& path_output, bool is_weighted){
    LOG("Saving the edge file " << path_output << " ...");
    Timer timer; timer.start();

    fstream out { path_output, ios::out | ios::binary };
    if(!out.good()) ERROR("Cannot create the file " << path_output);

    if(g_compress_output){ // compressed output
        constexpr uint64_t buffer_sz (1 << 20); // * sizeof(uint64_t)
        unique_ptr<uint64_t[]> ptr_input_buffer { new uint64_t[buffer_sz * 3 /* src + dst + weight */ ] };
        uint64_t* input_buffer = ptr_input_buffer.get();
        unique_ptr<uint64_t[]> ptr_output_buffer { new uint64_t[buffer_sz] };
        unsigned char* output_buffer = reinterpret_cast<unsigned char*>(ptr_output_buffer.get());
        uint64_t next_edge_id = 0;

        z_stream stream; int rc (0);
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = 0;
        stream.avail_out = 0;
        rc = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
        if(rc != Z_OK) ERROR("Cannot initialise the zlib stream");

        do {
            // input, the data that has to be compressed
            if(stream.avail_in == 0){
                uint64_t chunk_sz = min(edges.size() - next_edge_id, buffer_sz);
                uint64_t increment = is_weighted ? 3 : 2;
                for(uint64_t i = 0, j = 0; i < chunk_sz; i++, j += increment){
                    auto e = edges[next_edge_id +i];
                    input_buffer[j] = e.source();
                    input_buffer[j +1] = e.destination();
                    if(is_weighted){ reinterpret_cast<double*>(input_buffer)[j +2] = e.m_weight; }
                }
                next_edge_id += chunk_sz;

                stream.next_in = reinterpret_cast<decltype(stream.next_in)>(input_buffer);
                stream.avail_in = chunk_sz * sizeof(uint64_t) * increment;
            }

            // output, the data compressed by zlib
            stream.avail_out = buffer_sz * sizeof(uint64_t);
            stream.next_out = output_buffer;

            // invoke zlib
            rc = deflate(&stream, (next_edge_id >= edges.size()) ? Z_FINISH : Z_NO_FLUSH);
            if(rc != Z_OK && rc != Z_STREAM_END) ERROR("Compression error");

            // flush to disk
            uint64_t bytes_compressed = buffer_sz * sizeof(uint64_t) - stream.avail_out;
            out.write((char*) output_buffer, bytes_compressed);
        } while(stream.avail_in > 0 || next_edge_id < edges.size());

        rc = deflateEnd(&stream);
        if(rc != Z_OK) ERROR("Cannot close the zlib stream");

    } else { // plain output
        for(uint64_t i = 0, sz = edges.size(); i < sz; i++){
            out << edges[i].source() << " " << edges[i].destination();
            if(is_weighted){
                out << " " << edges[i].weight();
            }
            out << "\n";
        }
    }

    out.close();
    timer.stop();
    LOG("Edge file saved in " << timer);
}

static void parse_command_line_arguments(int argc, char* argv[]){
    using namespace cxxopts;

    Options options(argv[0], "Graphalytics vertex remapper (vtxremap): remap the vertices ID of the input graph into the dense domain [0, num_vertices)");
    options.custom_help(" [options] <input> <output>");
    options.add_options()
            ("c, compress", "Compress the output vertices and edges with zlib")
            ("h, help", "Show this help menu")
            ("s, sorted", "Respect the sorted order of the vertices in the mapping")
            ;

    auto parsed_args = options.parse(argc, argv);

    if( argc == 1 || parsed_args.count("help") > 0 ){
        cout << options.help() << endl;
        exit(EXIT_SUCCESS);
    }

    if( argc != 3 ) {
        INVALID_ARGUMENT("Invalid number of arguments: " << argc << ". Expected format: " << argv[0] << " [options] <input> <output>");
    }
    if(!common::filesystem::file_exists(argv[1])){
        INVALID_ARGUMENT("The given input graph does not exist: `" << argv[1] << "'");
    }

    g_path_input = argv[1];
    g_path_output = argv[2];
    g_compress_output = parsed_args.count("compress");
    g_sorted_order_vertices = parsed_args.count("sorted");

    cout << "Path input graph: " << g_path_input << "\n";
    cout << "Path output log: " << g_path_output << "\n";
    cout << "Compress the output with zlib: " << boolalpha << g_compress_output << "\n";
    cout << "Respect the sorted order: " << boolalpha << g_sorted_order_vertices << "\n";
    cout << endl;
}

static string get_current_datetime(){
    auto t = time(nullptr);
    if(t == -1){ ERROR("Cannot fetch the current time"); }
    auto tm = localtime(&t);
    char buffer[256];
    auto rc = strftime(buffer, 256, "%d/%m/%Y %H:%M:%S", tm);
    if(rc == 0) ERROR("strftime");
    return string(buffer);
}

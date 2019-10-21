/**
 * Copyright (C) 2019 Dean De Leo, email: dleo[at]cwi.nl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <random>
#include <unordered_map>

#include "lib/common/error.hpp"

/**
 * An exception raised by the reader while parsing the input files
 */
DEFINE_EXCEPTION(ReaderError);

/**
 * Parser to read the property file, the vertex and the edge list provided in the datasets from graphalytics.org
 * Initialise the reader by passing the file to the property file (.properties), the vertex and edge files are
 * derived from the parameters inside the property file.
 */
class GraphalyticsReader {
    std::unordered_map<std::string, std::string> m_properties; // property file
    bool m_directed = true; // whether the graph being processed is directed or not
    bool m_is_weighted = false; // whether the graph being processed contains weights or not
    void* m_handle_edge_file { nullptr }; // fstream, handle to parse the edge-file
    void* m_handle_vertex_file { nullptr }; // fstream, handle to parse the vertex-file
    uint64_t m_last_source {0}; uint64_t m_last_destination {0}; double m_last_weight{0.0}; // the last edge being parsed
    bool m_last_reported = true; // whether we have reported the last edge with source/dest vertices swapped in an undirected graph
    bool m_emit_directed_edges = false; // if the graph is undirected, report the same edge twice as src -> dest and dest -> src
    double m_max_weight = 1.0; // the maximum weight to generate when operating on non weighted graph
    std::mt19937 m_random_generator; // generator for the weights

    // Close the internal handles to parse the edge-file/vertex-file
    void close();

    /**
     * Check whether the given line is a comment or empty, that is, it starts with a sharp symbol # or contains no symbols
     */
    static bool ignore_line(const char* line);
    static bool ignore_line(const std::string& line);

    /**
     * Check whether the current marker points to a number
     */
    static bool is_number(const char* marker);

public:
    /**
     * Init the reader with the path to the graph property files (*.properties)
     */
    GraphalyticsReader(const std::string& path_properties, uint64_t seed = std::random_device{}());

    /**
     * Destructor
     */
    ~GraphalyticsReader();

    /**
     * Interface, report one edge at the time
     */
    bool read(uint64_t& out_source, uint64_t& out_destination, double& out_weight);

    /**
     * Iterator, read one edge at the time from the graph
     */
    bool read_edge(uint64_t& out_source, uint64_t& out_destination, double& out_weight);

    /**
     * Iterator, read one vertex at the time from the graph
     */
    bool read_vertex(uint64_t& out_vertex);

    /**
     * Reset the position of the iterators read/read_edge/read_vertex at the start of the file
     */
    void reset();

    // Retrieve the given property from the map, or the empty string if the property is not present
    std::string get_property(const std::string& key) const;

    /**
     * Path to the vertex file
     */
    std::string get_path_vertex_list() const;

    /**
     * Path to the edge file
     */
    std::string get_path_edge_list() const;

    /**
     * Check whether the graph is directed
     */
    bool is_directed() const;

    /**
     * Check whether the graph is weighted
     */
    bool is_weighted() const;

    /**
     * Shall we report the same edge twice in an undirected graph, once as src -> dest and once as dest -> src
     */
    void set_emit_directed_edges(bool value);

    /**
     * Set the max weight that can be generated when reading non weighted graphs
     */
     void set_max_weight(double value);
};
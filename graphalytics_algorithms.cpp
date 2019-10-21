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

#include "graphalytics_algorithms.hpp"

#include <algorithm>
#include "graphalytics_reader.hpp"

using namespace std;

GraphalyticsAlgorithms::GraphalyticsAlgorithms(GraphalyticsReader& props) {
    stringstream ss { props.get_property("algorithms") };
    string algorithm_name;
    while (getline(ss, algorithm_name, ',')){
        algorithm_name.erase(std::remove_if(begin(algorithm_name), end(algorithm_name), ::isspace), end(algorithm_name));
        transform(algorithm_name.begin(), algorithm_name.end(), algorithm_name.begin(), ::tolower);
        if(algorithm_name == "bfs"){
            bfs.m_enabled = true;
            bfs.m_source_vertex = stoull( props.get_property("bfs.source-vertex") );
        } else if (algorithm_name == "cdlp"){
            cdlp.m_enabled = true;
            cdlp.m_max_iterations = stoull( props.get_property("cdlp.max-iterations"));
        } else if (algorithm_name == "lcc"){
            lcc.m_enabled = true;
        } else if (algorithm_name == "pr") { // pagerank
            pagerank.m_enabled = true;
            pagerank.m_damping_factor = stod( props.get_property("pr.damping-factor"));
            pagerank.m_num_iterations = stoull( props.get_property("pr.num-iterations"));
        } else if(algorithm_name == "sssp"){
            sssp.m_enabled = true;
            sssp.m_source_vertex = stoull( props.get_property("sssp.source-vertex"));
        } else if(algorithm_name == "wcc"){
            wcc.m_enabled = true;
        }
    }
}

ostream& operator<<(std::ostream& out, const GraphalyticsAlgorithms& props){
    out << "[GraphalyticsAlgorithms";
    if(props.bfs.m_enabled){
        out << " BFS source: " << props.bfs.m_source_vertex << ";";
    }
    if(props.cdlp.m_enabled){
        out << " CDLP max_iterations: " << props.cdlp.m_max_iterations << ";";
    }
    if(props.lcc.m_enabled){
        out << " LCC;";
    }
    if(props.pagerank.m_enabled){
        out << " PageRank df: " << props.pagerank.m_damping_factor << ", num_iterations: " << props.pagerank.m_num_iterations << ";";
    }
    if(props.sssp.m_enabled){
        out << " SSSP source: " << props.sssp.m_source_vertex << ";";
    }
    if(props.wcc.m_enabled){
        out << " WCC;";
    }
    out << "]";
    return out;
}
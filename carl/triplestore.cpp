// Author: Thomas Pellissier Tanon

#include <iostream>
#include <fstream>
#include <sstream>

#include "triplestore.h"

void TripleStore::loadFile(const std::string &file_name) {
    std::ifstream input_stream(file_name);
    if (!input_stream.is_open()) {
        throw std::runtime_error(file_name + " is not readable.");
    }
    std::string s, p, o;
    size_t i = 0;
    std::string line;
    while (std::getline(input_stream, line)) {
        std::istringstream line_stream(line);
        if(!(line_stream >> s >> p >> o)) {
            continue;
        }
        addTriple(s, p, o);
        i++;
        if (i % 100000 == 0) {
            std::cout << '*' << std::flush;
        }
    }
    input_stream.close();
    std::cout << std::endl << i << " facts imported" << std::endl;
}

void TripleStore::addTriple(const std::string &subject, const std::string &predicate, const std::string &object) {
    const auto s = getIdForNode(subject);
    const auto p = getIdForNode(predicate);
    const auto o = getIdForNode(object);

    pso[p][s].insert(o);
    properties.insert(p);
}

TripleStore::node_id TripleStore::getIdForNode(const std::string &node) {
    if (node[0] == '<' && node[node.size() - 1] == '>') {
        return getIdForNode(node.substr(1, node.size() - 2));
    }

    if (!map_has_key(id_for_nodes, node)) {
        id_for_nodes[node] = nodes.size();
        nodes.push_back(node);
    }
    return id_for_nodes[node];
}

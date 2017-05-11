// Author: Thomas Pellissier Tanon

#pragma once

#include <vector>
#include <map>
#include <set>
#include <experimental/optional>

template <class _Key, class _Tp, class _Compare, class _Allocator>
inline bool map_has_key(const std::map<_Key, _Tp, _Compare, _Allocator>& map, const _Key& key) {
    return map.find(key) != map.end();
}

template <class _Key, class _Tp, class _Compare, class _Allocator>
inline std::experimental::optional<_Tp> map_get_value(const std::map<_Key, _Tp, _Compare, _Allocator>& map, const _Key& key) {
    const auto iter = map.find(key);
    if(iter == map.end()) {
        return {};
    } else {
        return (*iter).second;
    }
}

/*TODO template <class _Key, class _Tp, class _Compare, class _Allocator>
inline _Tp map_get_value(const std::map<_Key, _Tp, _Compare, _Allocator>& map, const _Key& key,  _Tp default_value ) {
    auto iter = map.find(key);
    if(iter == map.end()) {
        return default_value;
    } else {
        return (*iter).second;
    }
}*/

template <class _Key, class _Tp, class _Compare, class _Allocator>
inline const _Tp& map_get_value(const std::map<_Key, _Tp, _Compare, _Allocator>& map, const _Key& key,  const _Tp& default_value ) {
    const auto iter = map.find(key);
    if(iter == map.end()) {
        return default_value;
    } else {
        return (*iter).second;
    }
}

template <class _Key, class _Compare, class _Allocator>
inline bool set_contains(const std::set<_Key, _Compare, _Allocator>& set, const _Key& key) {
    return set.find(key) != set.end();
}


class TripleStore {
public:
    typedef unsigned int node_id;

    std::map<node_id, std::map<node_id, std::set<node_id>>> pso;

    void loadFile(const std::string &file_name);

    void addTriple(const std::string &subject, const std::string &predicate, const std::string &object);

    inline const std::string &getNodeForId(const TripleStore::node_id id) const {
        return nodes[id];
    }

    node_id getIdForNode(const std::string &node);

    inline size_t getNumberOfEntities() const {
        return nodes.size();
    }

    inline const std::set<node_id>& getProperties() const {
        return properties;
    }

    inline bool contains(const std::string &subject, const std::string &predicate, const std::string &object) {
        return contains(getIdForNode(subject), getIdForNode(predicate), getIdForNode(object));
    }

    inline bool contains(const TripleStore::node_id subject, const TripleStore::node_id predicate, const TripleStore::node_id object) {
        return map_has_key(pso[predicate], subject) && set_contains(pso[predicate][subject], object);
    }

private:
    std::vector<std::string> nodes;
    std::map<std::string, node_id> id_for_nodes;
    std::set<node_id> properties;
};

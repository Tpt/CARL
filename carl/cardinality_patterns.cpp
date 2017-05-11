// Author: Thomas Pellissier Tanon

#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <sstream>
#include <limits>
#include <memory>
#include <algorithm>
#include <cstdlib>

#include "triplestore.h"

const size_t MIN_STANDARD_CONFIDENCE_X100 = 1;
const size_t MAX_STANDARD_CONFIDENCE = 100;
const size_t MIN_SUPPORT = 200;
const size_t CARDINALITIES_UPPER_BOUND = 5;

struct Boundary {
    size_t count;
    TripleStore::node_id property;
    bool is_upper;

    Boundary(const TripleStore::node_id property, const size_t count, const bool is_upper) : count(count),
                                                                                             property(property),
                                                                                             is_upper(is_upper) {
    }

    bool operator==(const Boundary &other) const {
        return property == other.property && count == other.count && is_upper == other.is_upper;
    }

    bool operator!=(const Boundary &other) const {
        return !this->operator==(other);
    }
};

struct SubjectPredicateBoundary : public Boundary {
    char subject;

    SubjectPredicateBoundary(const char subject, const TripleStore::node_id property, const size_t count,
                             const bool is_upper) : Boundary(property, count, is_upper), subject(subject) {
    }

    bool operator==(const SubjectPredicateBoundary &other) const {
        return subject == other.subject && ((Boundary *) this)->operator==(other);
    }

    bool operator!=(const SubjectPredicateBoundary &other) const {
        return !this->operator==(other);
    }
};

struct TriplePattern {
    TripleStore::node_id property;
    char subject;
    char object;

    TriplePattern(const char subject, const TripleStore::node_id property, const char object) : property(property),
                                                                                                subject(subject),
                                                                                                object(object) {
    }
};

struct Rule {
    SubjectPredicateBoundary head;
    std::vector<SubjectPredicateBoundary> body_boundaries;
    std::vector<TriplePattern> body_triples;
    size_t support;
    size_t confidence;
    size_t contradictions;
    float contradictions_ratio;

    Rule(const SubjectPredicateBoundary &head) : head(head) {}

    Rule mergedWith(const Rule &rule) const {
        if (head != rule.head) {
            throw std::runtime_error("Invalid merging");
        }
        Rule new_rule = *this;
        new_rule.body_triples.insert(new_rule.body_triples.end(), rule.body_triples.begin(), rule.body_triples.end());
        new_rule.body_boundaries.insert(new_rule.body_boundaries.end(), rule.body_boundaries.begin(),
                                        rule.body_boundaries.end());
        return new_rule;
    }
};


class CardinalitiesStore {
public:
    size_t getUpperBound(const TripleStore::node_id s, const TripleStore::node_id p) {
        auto sp_bound = map_get_value(at_most_bounds_for_property_subject[p], s, std::numeric_limits<std::size_t>::max());
        return map_get_value(at_most_bounds_for_property, p, sp_bound);
    }

    size_t getLowerBound(const TripleStore::node_id s, const TripleStore::node_id p) {
        size_t default_value = getActualCount(s, p);
        auto sp_bound = map_get_value(at_least_bounds_for_property_subject[p], s, default_value);
        return map_get_value(at_least_bounds_for_property, p, sp_bound);
    }

    size_t getActualCount(const TripleStore::node_id s, const TripleStore::node_id p) {
        if (!map_has_key(pso[p], s)) {
            return 0;
        }
        return pso[p][s].size();
    }

    void loadFile(const std::string &file_name) {
        //TODO: bad hack
        at_least_bounds_for_property[getIdForNode("P22")] = 1;
        at_least_bounds_for_property[getIdForNode("P25")] = 1;
        at_most_bounds_for_property[getIdForNode("P22")] = 1;
        at_most_bounds_for_property[getIdForNode("P25")] = 1;
        possibles_at_most_bounds[getIdForNode("P22")].insert(1);
        possibles_at_least_bounds[getIdForNode("P22")].insert(1);
        possibles_at_most_bounds[getIdForNode("P25")].insert(1);
        possibles_at_least_bounds[getIdForNode("P25")].insert(1);

        std::ifstream input_stream(file_name);
        if (!input_stream.is_open()) {
            throw std::runtime_error(file_name + " is not readable.");
        }
        size_t i = 0;
        std::string s, p, o;
        std::string line;
        while (std::getline(input_stream, line)) {
            std::istringstream line_stream(line);
            if (!(line_stream >> s >> p >> o)) {
                continue;
            }
            if (p == "hasExactCardinality" || p == "hasAtLeastCardinality" || p == "hasAtMostCardinality") {
                std::string subject;
                size_t j = 0;
                for (; j < s.size() && s[j] != '|'; j++) {
                    subject.push_back(s[j]);
                }
                std::string predicate = s.substr(j + 1, std::string::npos);
                /*if (!set_contains(allowed_properties, predicate)) { //TODO not well parsed
                    continue;
                }*/
                auto p_ = getIdForNode(predicate);
                auto s_ = getIdForNode(subject);

                if(!set_contains(properties, p_)) { //We do not get cardinalities on properties we know nothing
                    continue;
                }

                size_t value = strtoul(o.c_str(), nullptr, 10);
                if (p == "hasExactCardinality") {
                    addAtLeastCardinality(s_, p_, value);
                    addAtMostCardinality(s_, p_, value);
                } else if (p == "hasAtLeastCardinality") {
                    addAtLeastCardinality(s_, p_, value);
                } else if (p == "hasAtMostCardinality") {
                    addAtMostCardinality(s_, p_, value);
                }
                individuals.insert(s_);
                properties.insert(p_);
            } else if (p == "http://www.w3.org/1999/02/22-rdf-syntax-ns#type") {
                if (o == "http://www.w3.org/2002/07/owl#FunctionalProperty") {
                    auto p_ = getIdForNode(s);
                    if(set_contains(properties, p_)) { //We do not get cardinalities on properties we know nothing
                        at_most_bounds_for_property[p_] = 1;
                        possibles_at_most_bounds[p_].insert(1);
                        possibles_at_least_bounds[p_].insert(1);
                    }
                }
            } else {
                const auto s_ = getIdForNode(s);
                const auto p_ = getIdForNode(p);
                const auto o_ = getIdForNode(o);

                pso[p_][s_].insert(o_);
                pos[p_][o_].insert(s_);
                individuals.insert(s_);
                properties.insert(p_);
                individuals.insert(o_);
            }
            i++;
            if (i % 100000 == 0) {
                std::cout << '*' << std::flush;
            }
        }
        std::cout << std::endl;
        input_stream.close();

        addBoundsFromStatements();
    }

    inline const std::string &getNodeForId(const TripleStore::node_id id) {
        return nodes[id];
    }

    TripleStore::node_id getIdForNode(const std::string &node) {
        if (node[0] == '<' && node[node.size() - 1] == '>') {
            return getIdForNode(node.substr(1, node.size() - 2));
        }

        if (!map_has_key(id_for_nodes, node)) {
            nodes.push_back(node);
            id_for_nodes[node] = nodes.size() - 1;
        }
        return id_for_nodes[node];
    }

    std::map<TripleStore::node_id, std::map<TripleStore::node_id, std::set<TripleStore::node_id>>> pso;
    std::map<TripleStore::node_id, std::map<TripleStore::node_id, std::set<TripleStore::node_id>>> pos;
    std::map<TripleStore::node_id, std::set<size_t>> possibles_at_least_bounds;
    std::map<TripleStore::node_id, std::set<size_t>> possibles_at_most_bounds;
    std::set<TripleStore::node_id> individuals;
    std::set<TripleStore::node_id> properties;

private:
    inline void addAtMostCardinality(TripleStore::node_id s, TripleStore::node_id p, size_t value) {
        at_most_bounds_for_property_subject[p][s] = value;
        if (value <= CARDINALITIES_UPPER_BOUND) {
            possibles_at_most_bounds[p].insert(value);
        }
    }

    inline void addAtLeastCardinality(TripleStore::node_id s, TripleStore::node_id p, size_t value) {
        if (value > 0) {
            at_least_bounds_for_property_subject[p][s] = value;
            if (value <= CARDINALITIES_UPPER_BOUND) {
                possibles_at_least_bounds[p].insert(value);
            }
        }
    }

    void addBoundsFromStatements() {
        //Extra bounds
        for (const auto p : properties) {
            possibles_at_most_bounds[p].insert(0);
        }

        for (const auto &psos : pso) {
            const auto p = psos.first;
            for (const auto &sos : psos.second) {
                size_t objects_number = sos.second.size();
                if (objects_number < CARDINALITIES_UPPER_BOUND) {
                    possibles_at_least_bounds[p].insert(objects_number);
                }
            }
        } //TODO*/
    }

    std::map<TripleStore::node_id, std::map<TripleStore::node_id, size_t>> at_least_bounds_for_property_subject;
    std::map<TripleStore::node_id, std::map<TripleStore::node_id, size_t>> at_most_bounds_for_property_subject;
    std::map<TripleStore::node_id, size_t> at_least_bounds_for_property;
    std::map<TripleStore::node_id, size_t> at_most_bounds_for_property;
    std::vector<std::string> nodes;
    std::map<std::string, TripleStore::node_id> id_for_nodes;
};

const unsigned int NO_VALUE = std::numeric_limits<unsigned int>::max();

class QueryTuple { //TODO support variables other than x and y
public:
    inline TripleStore::node_id getValue(char variable) const {
        switch (variable) {
            case 'x':
                return x_value;
            case 'y':
                return y_value;
            default:
                throw std::runtime_error(std::string(1, variable) + " variable name is not supported.");
        }
    }

    inline QueryTuple withValue(char variable, TripleStore::node_id value) const {
        QueryTuple new_tuple = *this;
        switch (variable) {
            case 'x':
                new_tuple.x_value = value;
                return new_tuple;
            case 'y':
                new_tuple.y_value = value;
                return new_tuple;
            default:
                throw std::runtime_error(std::string(1, variable) + " variable name is not supported.");
        }
    }

    inline QueryTuple withoutValue(char variable) const {
        if(isBinded(variable)) {
            return this->withValue(variable, NO_VALUE);
        } else {
            return *this;
        }
    }

    inline bool isBinded(char variable) const {
        switch (variable) {
            case 'x':
                return x_value != NO_VALUE;
            case 'y':
                return y_value != NO_VALUE;
            default:
                return false;
        }
    }

    inline bool operator<(const QueryTuple &other) const {
        return x_value < other.x_value || (x_value == other.x_value && y_value < other.y_value);
    }

    inline bool operator==(const QueryTuple &other) const {
        return x_value == other.x_value && y_value == other.y_value;
    }

private:
    TripleStore::node_id x_value = NO_VALUE;
    TripleStore::node_id y_value = NO_VALUE;
};


void addBoundaryToStream(const SubjectPredicateBoundary boundary, std::ostream &ostream,
                         std::shared_ptr<CardinalitiesStore> triples) {
    if (boundary.is_upper) {
        ostream << "C(" << triples->getNodeForId(boundary.property) << "(" << boundary.subject << ", _)) <= "
                << boundary.count;
    } else {
        ostream << "C(" << triples->getNodeForId(boundary.property) << "(" << boundary.subject << ", _)) >= "
                << boundary.count;
    }
}

void addRuleToStream(const Rule &rule, std::ostream &ostream, std::shared_ptr<CardinalitiesStore> triples) {
    addBoundaryToStream(rule.head, ostream, triples);
    ostream << " <-";
    for (const auto &triple : rule.body_triples) {
        ostream << ' ' + triples->getNodeForId(triple.property) << '(' << triple.subject << ", " << triple.object
                << ')';
    }
    for (const auto &boundary : rule.body_boundaries) {
        ostream << ' ';
        addBoundaryToStream(boundary, ostream, triples);
    }

}

class CardinalityRuleMining {
public:
    typedef std::map<std::pair<TripleStore::node_id,TripleStore::node_id>,std::pair<size_t,size_t>> map_id_id_size_t_size_t;

    CardinalityRuleMining(std::shared_ptr<CardinalitiesStore> triple_store)
            : cardinalityStore(triple_store), tuplesForIndividuals(std::vector<QueryTuple>()) {
        QueryTuple empty_tuple;
        for (const auto x : triple_store->individuals) {
            tuplesForIndividuals.push_back(empty_tuple.withValue('x', x));
        }
    }

    std::vector<Rule> doMining(size_t output_k_rules) {
        std::cout << "starting rule mining" << std::endl;
        std::cout << "doing mining on properties: ";
        for(const auto p : cardinalityStore->properties) {
            std::cout << cardinalityStore->getNodeForId(p) << ' ';
        }
        std::cout << std::endl;
        std::cout << "computing possible boundaries" << std::endl;

        //Possible cardinality boundaries
        std::vector<std::vector<SubjectPredicateBoundary>> possible_boundaries_with_priority_list; //For each inner vector the first element is a constrains included in the second...
        size_t number_of_boundaries = 0;
        for (const auto property_with_bounds : cardinalityStore->possibles_at_least_bounds) {
            std::vector<SubjectPredicateBoundary> priority_list;
            const auto property = property_with_bounds.first;
            for (auto bound_iterator = property_with_bounds.second.rbegin();
                 bound_iterator != property_with_bounds.second.rend(); bound_iterator++) {
                priority_list.push_back(SubjectPredicateBoundary('x', property, *bound_iterator, false));
            }
            number_of_boundaries += priority_list.size();
            possible_boundaries_with_priority_list.push_back(priority_list);
        }
        for (const auto property_with_bounds : cardinalityStore->possibles_at_most_bounds) {
            std::vector<SubjectPredicateBoundary> priority_list;
            const auto property = property_with_bounds.first;
            for (const auto bound : property_with_bounds.second) {
                priority_list.push_back(SubjectPredicateBoundary('x', property, bound, true));
            }
            number_of_boundaries += priority_list.size();
            possible_boundaries_with_priority_list.push_back(priority_list);
        }
        std::cout << number_of_boundaries << " possible boundaries" << std::endl;

        //Rule mining
        std::cout << "doing rule mining" << std::endl;
        std::vector<Rule> rules;

        //All possible heads
        std::vector<Rule> head_rules;
        for (const auto &boundary_list : possible_boundaries_with_priority_list) {
            for (const auto &boundary : boundary_list) {
                Rule rule(boundary);
                addEvaluations(rule);
                if (rule.support >= MIN_SUPPORT) {
                    head_rules.push_back(rule);
                    if (rule.confidence >= MIN_STANDARD_CONFIDENCE_X100) {
                        rules.push_back(rule);
                    }
                }
            }
        }

        //Optionally a C_x
        std::vector<Rule> rules_with_x_bounds;
        for (const auto &rule : head_rules) {
            //We add a C_N(X) with a new property (to avoid trivial rules C_N(X) <= k -> C_N(X) <= k' with k' >= k
            for (const auto &boundary_list : possible_boundaries_with_priority_list) {
                Rule parent_rule = rule;
                for (const auto &boundary : boundary_list) {
                    if (boundary.property != rule.head.property) {
                        Rule new_rule = rule;
                        new_rule.body_boundaries.push_back(boundary);
                        addEvaluations(new_rule);
                        if (new_rule.support >= MIN_SUPPORT && parent_rule.confidence < new_rule.confidence) {
                            rules_with_x_bounds.push_back(new_rule);
                            parent_rule = new_rule;
                            if (new_rule.confidence >= MIN_STANDARD_CONFIDENCE_X100) {
                                rules.push_back(new_rule);
                            }
                        }
                    }
                }
            }
        }

        //Optionally a P(X,Y) and a C_Y
        std::vector<Rule> rules_with_y_bounds;
        for (const auto &rule : head_rules) {
            for (const auto property : cardinalityStore->properties) {
                std::vector<Rule> new_rules(2, rule); //+p(x,y) and p(y,x)
                new_rules[0].body_triples.push_back(TriplePattern('x', property, 'y'));
                new_rules[1].body_triples.push_back(TriplePattern('y', property, 'x'));

                for (Rule &new_rule : new_rules) {
                    addEvaluations(new_rule);
                    Rule main_parent_rule = rule;
                    if (new_rule.support >= MIN_SUPPORT && rule.confidence < new_rule.confidence) {
                        rules_with_y_bounds.push_back(new_rule);
                        main_parent_rule = new_rule;
                        if (new_rule.confidence >= MIN_STANDARD_CONFIDENCE_X100) {
                            rules.push_back(new_rule);
                        }
                    }

                    //We add a C_M(Y)
                    for (const auto &boundary_list : possible_boundaries_with_priority_list) {
                        Rule parent_rule = main_parent_rule;
                        for (const auto &boundary : boundary_list) {
                            Rule new_rule2 = new_rule;
                            new_rule2.body_boundaries.push_back(
                                    SubjectPredicateBoundary('y', boundary.property, boundary.count,
                                                             boundary.is_upper));
                            addEvaluations(new_rule2);
                            if (new_rule2.support >= MIN_SUPPORT && parent_rule.confidence < new_rule2.confidence) {
                                rules_with_y_bounds.push_back(new_rule2);
                                parent_rule = new_rule2;
                                if (new_rule2.confidence >= MIN_STANDARD_CONFIDENCE_X100) {
                                    rules.push_back(new_rule2);
                                }
                            }
                        }
                    }
                }
            }
        }

        //We merges rules with X and Y bounds together
        size_t rules_count = head_rules.size() + rules_with_x_bounds.size() + rules_with_y_bounds.size();
        for (const auto &x_rule : rules_with_x_bounds) {
            for (const auto &y_rule : rules_with_y_bounds) {
                if (x_rule.head == y_rule.head) {
                    Rule new_rule = y_rule.mergedWith(x_rule);
                    if (x_rule.confidence < new_rule.confidence && y_rule.confidence < new_rule.confidence) {
                        if (new_rule.confidence >= MIN_STANDARD_CONFIDENCE_X100) {
                            rules.push_back(new_rule);
                        }
                    }
                    rules_count++;
                }
            }
        }

        std::cout << rules_count << " rules generated" << std::endl;
        std::cout << "sorting rules" << std::endl;
        std::sort(rules.begin(), rules.end(),
                  [](const Rule &a, const Rule &b) {
                      return a.confidence > b.confidence;
                  });

        size_t limit = std::min(output_k_rules, rules.size());
        std::vector<Rule> result;
        for (size_t i = 0; i < limit; i++) {
            result.push_back(rules[i]);
        }
        return result;
    }


    std::pair<map_id_id_size_t_size_t,map_id_id_size_t_size_t> executeRules(const std::vector<Rule>& rules) {
        map_id_id_size_t_size_t upper_bounds;
        map_id_id_size_t_size_t lower_bounds;

        for(const auto p : cardinalityStore->properties) { //TODO:memory greedy
            for(const auto x : cardinalityStore->individuals) {
                upper_bounds[std::make_pair(x, p)] = std::make_pair(cardinalityStore->getUpperBound(x, p), MAX_STANDARD_CONFIDENCE);
                lower_bounds[std::make_pair(x, p)] = std::make_pair(cardinalityStore->getLowerBound(x, p), MAX_STANDARD_CONFIDENCE);
            }
        }

        std::cout << cardinalityStore->individuals.size() << " individuals" << std::endl;
        size_t contradictions_sum = 0;
        std::pair<size_t,size_t> lower_default = std::make_pair(0, MAX_STANDARD_CONFIDENCE);
        std::pair<size_t,size_t> upper_default = std::make_pair(std::numeric_limits<std::size_t>::max(), MAX_STANDARD_CONFIDENCE);
        for(const auto& rule : rules) {
            size_t added_contradictions = 0;
            for (const auto& tuple : evaluateRuleBody(rule)) {
                auto x = tuple.getValue(rule.head.subject);
                auto p = rule.head.property;
                auto x_p = std::make_pair(x, p);
                const auto& lower_bound = map_get_value(lower_bounds, x_p, lower_default);
                const auto& upper_bound = map_get_value(upper_bounds, x_p, upper_default);
                if (rule.head.is_upper) {
                    if(lower_bound.first > rule.head.count) {
                        added_contradictions++;
                    } else if(upper_bound.first > rule.head.count) {
                        upper_bounds[x_p] = std::make_pair(rule.head.count, rule.confidence);
                    }
                } else {
                    if(upper_bound.second < rule.head.count) {
                        added_contradictions++;
                    } else if(lower_bound.first < rule.head.count) {
                        lower_bounds[x_p] = std::make_pair(rule.head.count, rule.confidence);
                    }
                }
            }
            contradictions_sum += added_contradictions;
            std::cout << added_contradictions << "\t" << contradictions_sum << "\t" << (float) rule.confidence / MAX_STANDARD_CONFIDENCE << "\t";
            addRuleToStream(rule, std::cout, cardinalityStore);
            std::cout << "\n";
        }

        return std::make_pair(lower_bounds, upper_bounds);
    }

    std::map<std::pair<TripleStore::node_id,TripleStore::node_id>,std::pair<size_t,size_t>> buildExactCardinalities(std::pair<map_id_id_size_t_size_t,map_id_id_size_t_size_t>& cardinalities) {
        std::map<std::pair<TripleStore::node_id,TripleStore::node_id>,std::pair<size_t,size_t>> exact_cardinalities;
        std::pair<size_t,size_t> upper_default = std::make_pair(std::numeric_limits<std::size_t>::max(), MAX_STANDARD_CONFIDENCE);
        for(const auto& lower_bound_per_x_p : cardinalities.first) {
            const auto& x_p = lower_bound_per_x_p.first;
            const auto& lower_bound = lower_bound_per_x_p.second;
            const auto& upper_bound =  map_get_value(cardinalities.second, x_p, upper_default);
            if(lower_bound.first == upper_bound.first) {
                exact_cardinalities[x_p] = std::make_pair(lower_bound.first, std::min(lower_bound.second, upper_bound.second));
            }
        }
        return exact_cardinalities;
    };

private:
    void addEvaluations(Rule &rule) {
        //We run the body to get all matching tuples
        std::set<QueryTuple> body_tuples;
        QueryTuple empty_tuple;
        std::vector<QueryTuple> body_query_tuples = evaluateRuleBody(rule);
        for (const auto &tuple : body_query_tuples) {
            if (tuple.isBinded('x')) {
                body_tuples.insert(tuple.withoutValue('y')); //TODO: support other variables
            } else {
                std::cout << "no x but with triple patterns" << std::endl;
                body_tuples.insert(tuplesForIndividuals.begin(), tuplesForIndividuals.end());
                break;
            }
        }

        size_t body_support = body_tuples.size();
        rule.support = 0;
        rule.contradictions = 0;
        rule.confidence = MAX_STANDARD_CONFIDENCE;
        rule.contradictions_ratio = 0;
        for (const auto &tuple : body_tuples) {
            if (matchesBoundary(tuple, rule.head)) {
                rule.support++;
            }
            if (contradictsBoundary(tuple, rule.head)) {
                rule.contradictions++;
            }
        }

        if(body_support > 0) {
            rule.confidence = (MAX_STANDARD_CONFIDENCE * rule.support) / body_support;
            rule.contradictions_ratio = (float) rule.contradictions / body_support;
        }
    }

    std::vector<QueryTuple> evaluateRuleBody(const Rule &rule) {
        if (rule.body_triples.empty()) {
            std::vector<QueryTuple> tuples;
            for (const auto &query_tuple : tuplesForIndividuals) { //TODO: optimize
                if (matchesBoundaries(query_tuple, rule.body_boundaries)) {
                    tuples.push_back(query_tuple);
                }
            }
            return tuples;
        }

        std::vector<QueryTuple> tuples(1);
        for (const auto &triple : rule.body_triples) {
            std::vector<QueryTuple> new_tuples;
            for (const auto &base_tuple : tuples) {
                if (base_tuple.isBinded(triple.subject)) {
                    if (base_tuple.isBinded(triple.object)) {
                        //We verify that the fact exists
                        if (set_contains(cardinalityStore->pso[triple.property][base_tuple.getValue(triple.subject)], base_tuple.getValue(triple.object))) {
                            new_tuples.push_back(base_tuple); //Already matches boundaries
                        }
                    } else {
                        //We do join on subject
                        for (const auto object : cardinalityStore->pso[triple.property][base_tuple.getValue(triple.subject)]) {
                            auto new_tuple = base_tuple.withValue(triple.object, object);
                            if (matchesBoundaries(new_tuple, rule.body_boundaries)) {
                                new_tuples.push_back(new_tuple);
                            }
                        }
                    }
                } else if (base_tuple.isBinded(triple.object)) {
                    //We do join on object
                    for (const auto subject : cardinalityStore->pos[triple.property][base_tuple.getValue(triple.object)]) {
                        auto new_tuple = base_tuple.withValue(triple.subject, subject);
                        if (matchesBoundaries(new_tuple, rule.body_boundaries)) {
                            new_tuples.push_back(new_tuple);
                        }
                    }
                } else {
                    for (const auto &sos : cardinalityStore->pso[triple.property]) {
                        for (const auto o : sos.second) {
                            auto new_tuple = base_tuple
                                    .withValue(triple.subject, sos.first)
                                    .withValue(triple.object, o);
                            if (matchesBoundaries(new_tuple, rule.body_boundaries)) {
                                new_tuples.push_back(new_tuple);
                            }
                        }
                    }
                }
            }
            tuples = new_tuples;
        }
        return tuples;
    }

    bool matchesBoundaries(const QueryTuple &tuple, const std::vector<SubjectPredicateBoundary> &boundaries) {
        for (const auto &boundary : boundaries) {
            if (!matchesBoundary(tuple, boundary)) {
                return false;
            }
        }
        return true;
    }

    bool matchesBoundary(const QueryTuple &tuple, const SubjectPredicateBoundary &boundary) {
        if (!tuple.isBinded(boundary.subject)) {
            return false;
        }
        if (boundary.is_upper) {
            return boundary.count >=
                   cardinalityStore->getUpperBound(tuple.getValue(boundary.subject), boundary.property);
        } else {
            return boundary.count <=
                   cardinalityStore->getLowerBound(tuple.getValue(boundary.subject), boundary.property);
        }
    }

    bool contradictsBoundary(const QueryTuple &tuple, const SubjectPredicateBoundary &boundary) {
        if (!tuple.isBinded(boundary.subject)) {
            return false;
        }
        if (boundary.is_upper) {
            return boundary.count <
                   cardinalityStore->getLowerBound(tuple.getValue(boundary.subject), boundary.property);
        } else {
            return boundary.count >
                   cardinalityStore->getUpperBound(tuple.getValue(boundary.subject), boundary.property);
        }
    }

    std::shared_ptr<CardinalitiesStore> cardinalityStore;
    std::vector<QueryTuple> tuplesForIndividuals;
};

int main(int argc, char *argv[]) {
    if (argc < 5) {
        std::cerr << argv[0] << " input_triples.tsv input_cardinalities.tsv output_rules.tsv output_cardinalities_directory" << std::endl;
        return EXIT_FAILURE;
    }
    std::string input_triples_file(argv[1]);
    std::string input_cardinalities_file(argv[2]);
    std::string output_rules_file(argv[3]);
    std::string output_cardinalities_directory(argv[4]);
    system(("mkdir -p " + output_cardinalities_directory).c_str());

    try {
        std::shared_ptr<CardinalitiesStore> triples = std::make_shared<CardinalitiesStore>();
        std::cout << "loading triples" << std::endl;
        triples->loadFile(input_triples_file);
        std::cout << "loading cardinalities" << std::endl;
        triples->loadFile(input_cardinalities_file);
        std::cout << triples->properties.size() << " properties and " << triples->individuals.size()
                  << " individuals loaded" << std::endl;

        CardinalityRuleMining ruleMining(triples);

        std::ofstream output_stream(output_rules_file);
        if (!output_stream.is_open()) {
            std::cerr << output_rules_file << " is not writable." << std::endl;
            return EXIT_FAILURE;
        }
        output_stream << "rule\tstandard_confidence\tnot_contradiction_ratio\n";
        const auto rules = ruleMining.doMining(1000);
        for (const auto &rule : rules) {
            addRuleToStream(rule, output_stream, triples);
            output_stream << "\t" << (float) rule.confidence / MAX_STANDARD_CONFIDENCE
                          << "\t" << 1 - rule.contradictions_ratio
                          << "\n";
        }
        output_stream.close();

        auto cardinalities = ruleMining.executeRules(rules);
        const auto new_cardinalities = ruleMining.buildExactCardinalities(cardinalities);
        for(size_t min_std_confidence = 0; min_std_confidence <= MAX_STANDARD_CONFIDENCE; min_std_confidence += 10) {
            std::string output_cardinalities_file = output_cardinalities_directory + "/" + std::to_string(min_std_confidence) + ".tsv";
            std::ofstream output_card_stream(output_cardinalities_file);
            if (!output_card_stream.is_open()) {
                std::cerr << output_cardinalities_file << " is not writable." << std::endl;
                return EXIT_FAILURE;
            }
            size_t complete = 0;
            size_t incomplete = 0;
            size_t missing_size = 0;
            for(const auto& new_cardinality : new_cardinalities) {
                TripleStore::node_id s = new_cardinality.first.first;
                TripleStore::node_id p = new_cardinality.first.second;
                size_t card = new_cardinality.second.second;
                if(new_cardinality.second.second >= min_std_confidence) {
                    output_card_stream << triples->getNodeForId(s) << '|' << triples->getNodeForId(p)
                                       << "\thasExactCardinality\t" << card << '\n';
                    size_t actualCard = triples->getActualCount(s, p);
                    if(actualCard >= card) {
                        complete++;
                    } else {
                        incomplete++;
                        missing_size += (actualCard - card);
                    }
                }
            }

            output_card_stream << "dataset\tcompleteCount\t" << complete << '\n';
            output_card_stream << "dataset\tincompleteCount\t" << incomplete << '\n';
            output_card_stream << "dataset\tmissingSize\t" << missing_size << '\n';

            size_t count_lower = 0;
            for(const auto& cardinality : cardinalities.first) {
                if(cardinality.second.second >= min_std_confidence) {
                    count_lower++;
                }
            }
            output_card_stream << "dataset\tlowerBoundNumber\t" << count_lower << '\n';
            size_t count_upper = 0;
            for(const auto& cardinality : cardinalities.second) {
                if(cardinality.second.second >= min_std_confidence) {
                    count_upper++;
                }
            }
            output_card_stream << "dataset\tupperBoundNumber\t" << count_upper << '\n';

            output_card_stream.close();
        }

        return EXIT_SUCCESS;
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

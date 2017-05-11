// Author: Thomas Pellissier Tanon

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <memory>
#include <sstream>

#include <stdlib.h>

#include "triplestore.h"

const double MIN_HEAD_COVERAGE = 0.001;
const double MIN_STANDARD_CONFIDENCE = 0.001;
const size_t MIN_SUPPORT = 10;
const double CONFIDENCE_INCOMPLETENESS_FACTOR = 0.5;


//p(x,y) /\ q(y,z) -> r(x,z)
struct ScoredRule {
    ScoredRule(const TripleStore::node_id p, const TripleStore::node_id q, const TripleStore::node_id r)
            : p(p), q(q), r(r) {
    }

    TripleStore::node_id p;
    TripleStore::node_id q;
    TripleStore::node_id r;

    size_t support = 0;
    size_t body_support = 0;
    double head_coverage;
    double standard_confidence;
    double pca_confidence;
    double completeness_confidence;
    double precision;
    double recall;
    double directional_metric;
    double directional_coef;
};

class CardinalitiesStore {
public:
    CardinalitiesStore(std::shared_ptr<TripleStore> triple_store) : triple_store(triple_store) {}

    inline bool hasExpectedCardinality(const TripleStore::node_id s, const TripleStore::node_id p) {
        return map_has_key(expected_cardinalities_by_property_value[p], s);
    }

    inline std::experimental::optional<size_t>
    getExpectedCardinality(const TripleStore::node_id s, const TripleStore::node_id p) {
        return map_get_value(expected_cardinalities_by_property_value[p], s);
    }

    void loadFile(const std::string &file_name) {
        std::ifstream input_stream(file_name);
        if (!input_stream.is_open()) {
            throw std::runtime_error(file_name + " is not readable.");
        }
        std::string s, p, o;
        std::string line;
        while (std::getline(input_stream, line)) {
            std::istringstream line_stream(line);
            if(!(line_stream >> s >> p >> o)) {
                continue;
            }
            if (p == "hasExactCardinality") {
                if(s.find_first_of('|') != std::string::npos) {
                    std::string subject;
                    size_t i = 0;
                    for (; i < s.size() && s[i] != '|'; i++) {
                        subject.push_back(s[i]);
                    }
                    std::string predicate = s.substr(i + 1, std::string::npos);
                    expected_cardinalities_by_property_value[triple_store->getIdForNode(predicate)]
                                                            [triple_store->getIdForNode(subject)] =
                                                                                        strtoul(o.c_str(), nullptr, 10);
                } else {
                    throw std::runtime_error("invalid hasExactCardinality subject");
                }
            }
        }
        input_stream.close();
    }

private:
    std::shared_ptr<TripleStore> triple_store;
    std::map<TripleStore::node_id, std::map<TripleStore::node_id, size_t>> expected_cardinalities_by_property_value;
};

class CardinalityRuleMining {
public:
    CardinalityRuleMining(std::shared_ptr<TripleStore> tripleStore,
                          std::shared_ptr<CardinalitiesStore> cardinalitiesStore) :
            tripleStore(tripleStore), cardinalityStore(cardinalitiesStore) {}

    std::vector<ScoredRule> doMining(size_t output_k_rules) {
        //Count number of triples per relations and number of entities
        std::set<TripleStore::node_id> entities;
        std::map<TripleStore::node_id, size_t> property_instances_count;
        size_t entity_count = tripleStore->getNumberOfEntities();
        for (const auto &pxy : tripleStore->pso) {
            for (const auto &xy : pxy.second) {
                property_instances_count[pxy.first] += xy.second.size();
            }
        }

        //Count the number of missing triples per relation
        std::map<TripleStore::node_id, size_t> number_of_expected_triple_per_relation;
        //std::map<TripleStore::node_id, size_t> number_of_incomplete_patterns;
        //std::map<TripleStore::node_id, size_t> number_of_complete_patterns;
        for (const auto property : tripleStore->getProperties()) {
            for (TripleStore::node_id subject = 0; subject < entity_count; subject++) { //TODO: bad hack to iterate on everything
                const auto& expected_cardinality = cardinalityStore->getExpectedCardinality(subject, property);
                if(expected_cardinality) {
                    auto actual_cardinality = map_has_key(tripleStore->pso[property], subject) ? tripleStore->pso[property][subject].size() : 0;
                    if (*expected_cardinality > actual_cardinality) {
                        number_of_expected_triple_per_relation[property] += *expected_cardinality - actual_cardinality;
                    }
                }
            }
        }

        //Compute support for each possible rule and each possible body
        std::vector<ScoredRule> rules;
        const std::set<TripleStore::node_id> empty_entity_set;
        for (const auto p : tripleStore->getProperties()) {
            for (const auto q : tripleStore->getProperties()) {
                for (const auto r : tripleStore->getProperties()) {

                    ScoredRule rule(p, q, r);
                    double pca_support = 0;

                    std::map<TripleStore::node_id, size_t> facts_added_by_subject_with_cardinality;
                    for (const auto &xy : tripleStore->pso[p]) {
                        const auto x = xy.first;

                        std::set<TripleStore::node_id> z_created;
                        for (const auto y : xy.second) {
                            const auto &new_z_created = map_get_value(tripleStore->pso[q], y, empty_entity_set);
                            z_created.insert(new_z_created.begin(), new_z_created.end());
                        }

                        if (!z_created.empty()) {
                            const auto &z_actual = map_get_value(tripleStore->pso[r], x, empty_entity_set);
                            const auto expects_cardinality = cardinalityStore->hasExpectedCardinality(x, r);
                            rule.body_support += z_created.size();
                            if(!z_actual.empty()) {
                                pca_support += z_created.size();
                            }
                            for(const auto z : z_created) {
                                if(set_contains(z_actual, z)) {
                                    rule.support++;
                                } else if(expects_cardinality) {
                                    facts_added_by_subject_with_cardinality[x]++;
                                }
                            }
                        }
                    }
                    if (rule.support < MIN_SUPPORT) {
                        continue;
                    }

                    rule.head_coverage = (double) rule.support / property_instances_count[rule.r];
                    if (rule.head_coverage < MIN_HEAD_COVERAGE) {
                        continue;
                    }

                    rule.standard_confidence = (double) rule.support / rule.body_support;
                    if (rule.standard_confidence < MIN_STANDARD_CONFIDENCE) {
                        continue;
                    }

                    rule.pca_confidence = (double) rule.support / pca_support;

                    size_t triple_added_to_missing_places_count = 0;
                    size_t triple_added_to_complete_places_count = 0;
                    for (const auto &t : facts_added_by_subject_with_cardinality) {
                        auto expected_cardinality = cardinalityStore->getExpectedCardinality(t.first, rule.r);
                        if (expected_cardinality) {
                            size_t actual_triples_number = tripleStore->pso[rule.r][t.first].size();
                            size_t missing_triples = 0;
                            if (*expected_cardinality > actual_triples_number) {
                                //To make sure it's >= 0 in case there is an inconsistency with number of triples
                                missing_triples = *expected_cardinality - actual_triples_number;
                            }
                            size_t triples_added_by_the_rule = t.second;
                            if (triples_added_by_the_rule > missing_triples) {
                                triple_added_to_missing_places_count += missing_triples;
                                triple_added_to_complete_places_count += triples_added_by_the_rule - missing_triples;
                            } else {
                                triple_added_to_missing_places_count += triples_added_by_the_rule;
                            }
                        } else {
                            std::cout << "Warning: stored added facts where no cardinality exists" << std::endl;
                        }
                    }

                    rule.completeness_confidence = rule.support / (double) (rule.body_support - triple_added_to_missing_places_count);

                    rule.precision = 1 - (double) triple_added_to_complete_places_count / rule.body_support;
                    if (number_of_expected_triple_per_relation[rule.r]) {
                        rule.recall = (double) triple_added_to_missing_places_count / number_of_expected_triple_per_relation[rule.r];
                    } else {
                        rule.recall = std::numeric_limits<double>::quiet_NaN();
                    }
                    if (triple_added_to_complete_places_count + triple_added_to_missing_places_count) {
                        rule.directional_metric =
                                ((double) triple_added_to_missing_places_count - triple_added_to_complete_places_count) /
                                        (2 * (triple_added_to_missing_places_count + triple_added_to_complete_places_count)) + 0.5;
                    } else {
                        rule.directional_metric = std::numeric_limits<double>::quiet_NaN();
                    }

                    /*double expected_complete = (double) number_of_complete_patterns[rule.r] / entity_count;
                    double actual_complete = (double) complete_triple_pattern_modified / subject_support;
                    double expected_incomplete = (double) number_of_incomplete_patterns[rule.r] / entity_count;
                    double actual_incomplete = (double) incomplete_triple_pattern_modified / subject_support;
                    if(actual_complete == 0) {
                        rule.directional_coef = std::numeric_limits<float>::max(); //We assume +\infty for expected_complete / actual_complete
                    } else if(expected_incomplete == 0) {
                        rule.directional_coef = std::numeric_limits<float>::max(); //We assume +\infty for actual_incomplete / expected_incomplete
                    } else {
                        rule.directional_coef = ((expected_complete / actual_complete) + (actual_incomplete / expected_incomplete)) / 2;
                    }*/

                    double possible_relations_num = entity_count*entity_count;
                    double expected_incomplete = number_of_expected_triple_per_relation[rule.r] / possible_relations_num;
                    double expected_complete = (possible_relations_num - number_of_expected_triple_per_relation[rule.r] - property_instances_count[rule.r]) / possible_relations_num;
                    double actual_complete = (double) triple_added_to_complete_places_count / rule.body_support;
                    double actual_incomplete = (double) triple_added_to_missing_places_count / rule.body_support;
                    if(actual_complete == 0 || expected_incomplete == 0) {
                        rule.directional_coef = std::numeric_limits<float>::max();
                    } else {
                        rule.directional_coef = 0.5 * expected_complete / actual_complete + 0.5 * actual_incomplete / expected_incomplete;
                    }

                    rules.push_back(rule);

                    std::cout << '*' << std::flush;
                }
            }
        }
        std::cout << std::endl << "starting output" << std::endl;

        std::sort(rules.begin(), rules.end(),
                  [](const ScoredRule &a, const ScoredRule &b) {
                      return a.completeness_confidence > b.completeness_confidence;
                  });

        size_t limit = std::min(output_k_rules, rules.size());
        std::vector<ScoredRule> result;
        for (size_t j = 0; j < limit; j++) {
            result.push_back(rules[j]);
        }
        return result;
    }

private:
    std::shared_ptr<TripleStore> tripleStore;
    std::shared_ptr<CardinalitiesStore> cardinalityStore;
};

double evaluate_rule(const ScoredRule& rule, std::shared_ptr<TripleStore> train_triples, std::shared_ptr<TripleStore> eval_triples) {
    size_t rule_support = 0;
    size_t body_support = 0;
    for (auto &xy : train_triples->pso[rule.p]) {
        const auto x = xy.first;
        std::set<TripleStore::node_id> z_created;
        for(const auto y : xy.second) {
            const auto& z_add = train_triples->pso[rule.q][y];
            z_created.insert(z_add.begin(), z_add.end());
        }
        for(const auto z : z_created) {
            if(!train_triples->contains(x, rule.r, z)) {
                if(eval_triples->contains(train_triples->getNodeForId(x), train_triples->getNodeForId(rule.r), train_triples->getNodeForId(z))) {
                    rule_support++;
                }
                body_support++;
            }
        }
    }
    return (double) rule_support / (double) body_support;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        std::cerr << argv[0] << " input_triples.tsv input_cardinalities.tsv evaluation_triples.tsv output.tsv" << std::endl;
        return EXIT_FAILURE;
    }
    std::string input_triples_file(argv[1]);
    std::string input_cardinalities_file(argv[2]);
    std::string eval_triples_file(argv[3]);
    std::string output_file(argv[4]);

    try {
        std::shared_ptr<TripleStore> input_triples = std::make_shared<TripleStore>();
        input_triples->loadFile(input_triples_file);

        std::shared_ptr<CardinalitiesStore> input_cardinalities = std::make_shared<CardinalitiesStore>(input_triples);
        input_cardinalities->loadFile(input_cardinalities_file);

        std::shared_ptr<TripleStore> eval_triples = std::make_shared<TripleStore>();
        eval_triples->loadFile(eval_triples_file);

        CardinalityRuleMining ruleMining(input_triples, input_cardinalities);
        auto result = ruleMining.doMining(1000);

        std::ofstream output_stream(output_file);
        if (!output_stream.is_open()) {
            std::cerr << output_file << " is not writable." << std::endl;
            return EXIT_FAILURE;
        }
        output_stream << "p\tq\tr\tsupport\tbody support\thead coverage\tstd conf\tpca conf\tcompl conf\tprecision\trecall\tdir metric\tdir coef\trule eval\n";
        for (const auto &rule : result) {
            output_stream << input_triples->getNodeForId(rule.p) << "\t" << input_triples->getNodeForId(rule.q)
                          << "\t" << input_triples->getNodeForId(rule.r) << "\t" << rule.support << "\t"
                          << rule.body_support << "\t" << rule.head_coverage << "\t"
                          << rule.standard_confidence << "\t" << rule.pca_confidence << "\t"
                          << rule.completeness_confidence << "\t"
                          << rule.precision << "\t" << rule.recall << "\t"
                          << rule.directional_metric << "\t" << rule.directional_coef << "\t"
                          << evaluate_rule(rule, input_triples, eval_triples) << "\n";
        }
        output_stream.close();
        return EXIT_SUCCESS;
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

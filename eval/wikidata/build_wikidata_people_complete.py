from itertools import chain
import gzip

import plac

def parse_triples(file_name):
    with gzip.open(file_name, 'rt') as file:
        for line in file:
            parts = [e.strip(' \r<>')
                         .replace('http://www.wikidata.org/prop/direct/', '')
                         .replace('http://www.wikidata.org/entity/', '') for e in line.strip(' \r\n.').split('\t')]
            yield parts


class TripleStore:
    def __init__(self):
        self.pso = {}

    def insert(self, s, p, o):
        if p not in self.pso:
            self.pso[p] = {}
        if s not in self.pso[p]:
            self.pso[p][s] = set()
        self.pso[p][s].add(o)

    def contains(self, s, p, o):
        return p in self.pso and s in self.pso[p] and o in self.pso[p][s]

allowed_predicates = {'P22', 'P25', 'P26', 'P40', 'P3373', 'P3448', 'P19', 'P20', 'P27'}

def main(input_file, output_file, output_cardinalities):
    triples = TripleStore()
    dead = set()
    people = set()
    gender = {}
    for (s, p, o) in parse_triples(input_file):
        if p in allowed_predicates:
            triples.insert(s, p, o)
            people.add(s)
        elif p == 'P570':
            dead.add(s)
            people.add(s)
        elif p == 'P21':
            gender[s] = o

    # father/mother for children

    for s,os in triples.pso['P40'].items():
        if s in gender:
            if gender[s] == 'Q6581097':  # is male
                for o in os:
                    triples.insert(o, 'P22', s)
            if gender[s] == 'Q6581072':  # is female
                for o in os:
                    triples.insert(o, 'P25', s)

    # children for father/mother
    for s,os in chain(triples.pso['P22'].items(), triples.pso['P25'].items()):
        for o in os:
            triples.insert(o, 'P40', s)

    # siblings using children (it's already complete)
    for s,os in triples.pso['P40'].items():
        for o1 in os:
            for o2 in os:
                if o1 != o2:
                    triples.insert(o1, 'P3373', o2)

    # symmetric relations
    for p in ['P3373', 'P26']:
        new_values = []
        for s,os in triples.pso[p].items():
            for o in os:
                new_values.append((o, s))
        for s,o in new_values:
            triples.insert(s, p, o)

    # we output everything
    counts = {}
    with open(output_file, 'wt') as file:
        for p, sos in triples.pso.items():
            counts[p] = 0
            for s, os in sos.items():
                for o in os:
                    file.write('{}\t{}\t{}\n'.format(s, p, o))
                    counts[p] += 1
    print(counts)

    # we build cardinalities
    with open(output_cardinalities, 'wt') as file:
        # functional mandatory predicates
        for p in {'P22', 'P25', 'P19'}:
            for s in people:
                file.write('{}|{}\thasExactCardinality\t1\n'.format(s, p))

        # death place
        for s in people:
            if s in triples.pso['P20'] or s in dead:
                file.write('{}|P20\thasExactCardinality\t1\n'.format(s))

        # properties assumed complete
        for p in {'P26', 'P40', 'P3373', 'P3448'}:
            for s, os in triples.pso[p].items():
                file.write('{}|{}\thasExactCardinality\t{}\n'.format(s, p, len(os)))

if __name__ == '__main__':
    plac.call(main)

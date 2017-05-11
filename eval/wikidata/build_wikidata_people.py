import gzip

import plac

def parse_triples(file_name):
    with gzip.open(file_name, 'rt') as file:
        for line in file:
            parts = [e.strip(' \r<>')
                         .replace('http://www.wikidata.org/prop/direct/', '')
                         .replace('http://www.wikidata.org/entity/', '') for e in line.strip(' \r\n.').split('\t')]
            yield parts


allowed_predicates = {'P22', 'P25', 'P26', 'P40', 'P3373', 'P3448', 'P19', 'P20', 'P27'}

def main(input_file, output_file):
    with open(output_file, 'wt') as file:
        for (s, p, o) in parse_triples(input_file):
            if p in allowed_predicates:
                file.write('{}\t{}\t{}\n'.format(s, p, o))

if __name__ == '__main__':
    plac.call(main)

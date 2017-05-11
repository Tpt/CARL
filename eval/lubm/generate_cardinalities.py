import plac


def parse_triples(file_name):
    with open(file_name, 'rt') as file:
        for line in file:
            parts = [e.strip(' \r\n<>') for e in line.split('\t')]
            if len(parts) != 3:
                print('Invalid line: {}'.format(line))
            else:
                yield parts


def main(input_file, output_file):
    counts = {}
    with open(output_file, 'wt') as output:
        for s, p, o in parse_triples(input_file):
            if p not in counts:
                 counts[p] = {}
            if s not in counts[p]:
                 counts[p][s] = 0
            counts[p][s] += 1
        for p,sc in counts.items():
            for s,c in sc.items():
               output.write("{}|{}\thasExactCardinality\t{}\n".format(s, p, c))


if __name__ == '__main__':
    plac.call(main)

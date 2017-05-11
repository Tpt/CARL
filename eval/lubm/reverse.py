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
    with open(output_file, 'wt') as output:
        for s, p, o in parse_triples(input_file):
            output.write("{}\t{}\t{}\n{}\t{}R\t{}\n".format(s, p, o, o, p, s))


if __name__ == '__main__':
    plac.call(main)


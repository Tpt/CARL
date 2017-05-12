import csv
import os
import plac
import random
import shutil
import subprocess
import tempfile


def parse_triples(file_name):
    with open(file_name, 'rt') as file:
        for line in file:
            parts = [e.strip(' \r\n<>') for e in line.split('\t')]
            if len(parts) != 3:
                print('Invalid line: {}'.format(line))
            else:
                yield parts


def main(input_triples, input_cardinalities, output_file):
    temp_dir = tempfile.mkdtemp()
    print("Working directory: " + temp_dir)

    try:
        input_train_file = temp_dir + '/train.tsv'
        input_test_file = temp_dir + '/test.tsv'
        output_temp_dir = temp_dir + '/out'

        with open(input_train_file, 'wt') as output_train:
            with open(input_test_file, 'wt') as output_test:
                count_train = 0
                count_test = 0
                for s, p, o in parse_triples(input_cardinalities):
                    if p == "hasExactCardinality":
                        in_train = (random.randrange(100) < 80)
                    else:
                        in_train = True
                    if in_train:
                        count_train += 1
                        output_train.write("{}\t{}\t{}\n".format(s, p, o))
                    else:
                        count_test += 1
                        output_test.write("{}\t{}\t{}\n".format(s, p, o))

        print('{} train and {} test cardinalities'.format(count_train, count_test))

        subprocess.check_call([
            "build/carl-cardinality_patterns",
            input_triples, input_train_file, temp_dir + "/rules.tsv", output_temp_dir
        ])

        test = {}
        for s, p, o in parse_triples(input_test_file):
            if p == "hasExactCardinality":
                test[s] = o

        with open(output_file, 'wt') as out:
            writer = csv.DictWriter(out,
                                    fieldnames=['minimal_confidence', 'recall', 'precision', 'f1', 'count_equalities',
                                                'lower_bounds_count', 'upper_bounds_count', 'complete_count',
                                                'incomplete_count', 'missing_size'], delimiter='\t')
            writer.writeheader()
            for file in os.listdir(output_temp_dir):
                count_matched = 0
                count_in_test = 0
                count_all = 0
                lower_bounds_count = 0
                upper_bounds_count = 0
                complete_count = 0
                incomplete_count = 0
                missing_size = 0
                for s, p, o in parse_triples(output_temp_dir + '/' + file):
                    if p == "hasExactCardinality":
                        if s in test:
                            if o == test[s]:
                                count_matched += 1
                            count_in_test += 1
                        count_all += 1
                    elif p == "lowerBoundNumber":
                        lower_bounds_count = int(o)
                    elif p == "upperBoundNumber":
                        upper_bounds_count = int(o)
                    elif p == "completeCount":
                        complete_count = int(o)
                    elif p == "incompleteCount":
                        incomplete_count = int(o)
                    elif p == "missingSize":
                        missing_size = int(o)

                data = {
                    'minimal_confidence': "{0:.1f}".format(float(file.replace('.tsv', '')) / 100),
                    'recall': float('nan'),
                    'precision': float('nan'),
                    'f1': float('nan'),
                    'count_equalities': count_all,
                    'lower_bounds_count': lower_bounds_count,
                    'upper_bounds_count': upper_bounds_count,
                    'complete_count': complete_count,
                    'incomplete_count': incomplete_count,
                    'missing_size': missing_size
                }
                if test and count_in_test:
                    recall = count_in_test / len(test)
                    precision = count_matched / count_in_test
                    f1 = 2 * (precision * recall) / (precision + recall)
                    data['precision'] = "{0:.4f}".format(precision)
                    data['recall'] = "{0:.4f}".format(recall)
                    data['f1'] = "{0:.4f}".format(f1)
                writer.writerow(data)
    finally:
        shutil.rmtree(temp_dir)


if __name__ == '__main__':
    plac.call(main)

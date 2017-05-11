import random
import tempfile
import shutil
import subprocess
import plac
import csv
import math
import numpy


def parse_triples(file_name):
    with open(file_name, 'rt') as file:
        for line in file:
            parts = [e.strip(' \r\n<>') for e in line.split('\t')]
            if len(parts) != 3:
                print('Invalid line: {}'.format(line))
            else:
                yield parts


def map_list_field(map_, field):
    return list(map(lambda x: float(x[field]), map_))

def correl(map_, field1, field2):
    try:
        return numpy.corrcoef(map_list_field(map_, field1), map_list_field(map_, field2))[0, 1]
    except IndexError:
        return 'nan'

def main(input_triples, input_cardinalities, output_file):
    temp_dir = tempfile.mkdtemp()

    try:
        input_train_file = temp_dir + '/train.tsv'
        input_test_file = temp_dir + '/test.tsv'
        output_temp_file = temp_dir + '/out.tsv'

        results = []

        for factor in [0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]:
            print('Doing factor {}'.format(factor))
            with open(input_train_file, 'wt') as output_train:
                with open(input_test_file, 'wt') as output_test:
                    test_size = 0
                    train_size = 0
                    train_size_R = 0
                    for s, p, o in parse_triples(input_triples):
                        if p[len(p) - 1] == 'R':
                            in_train = (random.randrange(100) <= 50*factor)
                            if in_train:
                                 train_size_R += 1
                        else:
                            in_train = (random.randrange(100) <= 100*factor)
                            if in_train:
                                 train_size += 1
                        if in_train:
                            output_train.write("{}\t{}\t{}\n".format(s, p, o))
                        else:
                            test_size += 1
                            output_test.write("{}\t{}\t{}\n".format(s, p, o))
            print('train: direct: {} reverse: {} and test: {}'.format(train_size, train_size_R, test_size))
            subprocess.check_call([
                "build/carl-patterns_using_cardinalities",
                input_train_file, input_cardinalities, input_test_file, output_temp_file
            ])

            rows = []
            with open(output_temp_file, 'rt', newline='') as output_learn:
                learn_reader = csv.DictReader(output_learn, delimiter='\t')
                for row in learn_reader:
                    #row['dir coef'] = row['dir coef'] if math.isnan(float(row['dir coef'])) else math.log(1 + float(row['dir coef']))
                    row['dir metric score'] = float(row['std conf']) if math.isnan(float(row['dir metric'])) else (float(row['dir metric']) + float(row['std conf'])) / 2
                    row['dir coef score'] = float(row['std conf']) if math.isnan(float(row['dir coef'])) else (float(row['dir coef']) + float(row['std conf'])) / 2
                    row['f1 score'] = 2 * (float(row['precision']) * float(row['recall'])) / (float(row['precision']) + float(row['recall']))
                    rows.append(row)
            results.append({
                'factor': factor,
                'std_conf_correl': correl(rows, 'std conf', 'rule eval'),
                'pca_conf_correl': correl(rows, 'pca conf', 'rule eval'),
                'compl_conf_correl': correl(rows, 'compl conf', 'rule eval'),
                'precision_correl': correl(rows, 'precision', 'rule eval'),
                'recall_correl': correl(rows, 'recall', 'rule eval'),
                'f1_correl': correl(rows, 'f1 score', 'rule eval'),
                'dir_metric_score_correl': correl(rows, 'dir metric score', 'rule eval'),
                'dir_coef_score_correl': correl(rows, 'dir coef score', 'rule eval'),
                'dir_metric_correl': correl(rows, 'dir metric', 'rule eval'),
                'dir_coef_correl': correl(rows, 'dir coef', 'rule eval'),
                'train_size': train_size + train_size_R,
                'test_size': test_size,
                'rules_count': len(rows),
                'support': sum(map_list_field(rows, 'support')),
                'body_support': sum(map_list_field(rows, 'body support')),
                'body_support_average': int(numpy.mean(map_list_field(rows, 'body support'))),
                'support_average': int(numpy.mean(map_list_field(rows, 'support')))
            })

        with open(output_file, 'wt') as out:
            writer = csv.DictWriter(out, fieldnames=list(results[0].keys()), delimiter='\t')
            writer.writeheader()
            for result in results:
                writer.writerow(result)

    finally:
        shutil.rmtree(temp_dir)


if __name__ == '__main__':
    plac.call(main)
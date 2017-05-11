CARL
====

CARL stands for Completeness-Aware Rule Learning and is an experimental rule learning system for knowledge base that uses cardinality as input. It have been created to experiments new rule learning techniques.

It provides two executable to learn new cardinalities from existing ones and to learn graph patterns using cardinalities.

## Install

CARL is written in plain C++14 and requires a compiler compatible with this version of C++ (recent version of GCC or Clang are fine) and is built using cmake.
It has no dependencies outside of the C++14 standard library and is compatible with GNU/Linux and macOS.

To compile it into the build directory, just run:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

The helper scripts are written in Python3 and requires the plac library (install it using `pip3 install plac`).

## Mine rules
To mine rules run:
```
./carl-patterns_using_cardinalities input_triples.tsv input_cardinalities.tsv evaluation_triples.tsv output.tsv
```

Where:
* `input_triples.tsv` is a file with the available knowledge base content structured as one (subject, predicate, object) triple per line, each of its part separated by a tabulation.
* `input_cardinalities.tsv` is a file with triples cardinalities. Exact cardinalities are represented as `SUBJECT|PREDICATE	hasExactCardinality	X` (note the `|` between `SUBJECT` and `PREDICATE`). Lower and upper bounds are represented using the same notation with the relations `hasAtLeastCardinality` and `hasAtMostCardinality`.
* `input_triples.tsv` is a file with the test triples in the same format as `input_triples.tsv`. Use `/dev/null` if you do not want evaluation.
* `output.tsv` is the file that will receive the mined rules.


## Mine cardinalities
To mine cardinalities run:
```
./carl-cardinality_patterns input_triples.tsv input_cardinalities.tsv output_rules.tsv output_cardinalities_directory
```

Where:
* `input_triples.tsv` is a file with the knowledge base content structured as one (subject, predicate, object) triple per line, each of its part separated by a tabulation.
* `input_cardinalities.tsv` is a file with triples cardinalities. Exact cardinalities are represented as `SUBJECT|PREDICATE	hasExactCardinality	X` (note the `|` between `SUBJECT` and `PREDICATE`). Lower and upper bounds are represented using the same notation with the relations `hasAtLeastCardinality` and `hasAtMostCardinality`.
* `output_rules.tsv` is the file that will receive the mined rules.
* `output_cardinalities_directory` is the directory that will receive the mined cardinalities for different thresholds of confidence.

If you want to retrieve some evaluation about this script you could use `python3 cardinalities_mining.py input_triples.tsv input_cardinalities.tsv output_file.tsv`

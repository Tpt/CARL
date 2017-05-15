LUBM experiments
================

This experiment is based on the [LUBM benchmark](http://swat.cse.lehigh.edu/projects/lubm/) and its UBA data generator.

## Building the triples file

Code here requires Maven and Java 8.

You have first to use UBA to generate OWL files. For that you have to run in the `lubm` directory:
```
mvn clean install exec:java -Dexec.mainClass="edu.lehigh.swat.bench.uba.Generator" -Dexec.args="-onto http://swat.cse.lehigh.edu/onto/univ-bench.owl -univ K"
```
with `K` the number of universities you want to generate (something around `5` is fine).

Then to convert the `.owl` files to plain text triple file `lubm_dataset.tsv` and expend them using the OWL reasoner:
```
 mvn clean install exec:java -Dexec.mainClass="de.mpg.mpiinf.uba.OWL2NT" -Dexec.args=". lubm_dataset.tsv"
```
If you do not want to run the reasoner just add the extra `-no-expand` argument inside of the `-Dexec.args` quotes (reasoning could take days).

## Rule mining with cardinalities

Optional : to add reverse predicates to the dataset run:
```
python3 reverse.py lubm_dataset.tsv lubm_dataset_with_reverse.tsv
```

To generate cardinalities file run:
```
python3 generate_cardinalities.py lubm_dataset.tsv lubm_cardinalities.tsv
```

Then you can run the experimentation with
```
python3 patterns_using_cardinalities_lubm.py ../eval/lubm/lubm_dataset.tsv ../eval/lubm/lubm_cardinalities.tsv rule_learning_statistics.tsv
```

This should be done in the `carl` directory.

The results generated from a dataset with 10 universities is provided in the [`results/rule_learning_statistics_10_univ.tsv`](results/rule_learning_statistics_10_univ.tsv) file.
Wikidata experiments
================

You should download a Wikidata truthy NTriples dump.
They are available at: https://dumps.wikimedia.org/wikidatawiki/entities/ and have names like `wikidata-YYYYMMDD-truthy-BETA.nt.gz`.

## Rule mining with cardinalities

To generate the completed graph for people on Wikidata with its cardinalities you have to run:
```
python3 build_wikidata_people_complete wikidata-YYYYMMDD-truthy-BETA.nt.gz wikidata_people_triples_full.tsv wikidata_people_triples_cardinalities.tsv
```
`wikidata_people_triples_full.tsv` is going to store the completed graph and `wikidata_people_cardinalities.tsv` its cardinalities.

Then you can run the experimentation with
`python3 patterns_using_cardinalities_people.py ../eval/wikidata/wikidata_people_triples_full.tsv ../eval/wikidata/wikidata_people_cardinalities.tsv output_file.tsv`
This should be done in the `carl` directory.


## Cardinality mining

To filter and simplify triples related ot people you should run:
```
python3 build_wikidata_people wikidata-YYYYMMDD-truthy-BETA.nt.gz wikidata_people_triples.tsv
```

Then you can run the experimentation with
```
python3 cardinalities_mining.py ../eval/wikidata/input_triples.tsv ../eval/wikidata/cardinalities.tsv output_file.tsv
```
This should be done in the `carl` directory.

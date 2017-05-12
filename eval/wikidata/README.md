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

This executes the following rules in this order to complete the graph:

* hasFather(x, y) <- hasChild(y, x), hasGender(y, male)
* hasMother(x, y) <- hasChild(y, x), hasGender(y, female)
* hasChild(x, y) <- hasFather(y, x)
* hasChild(x, y) <- hasMother(y, x)
* hasSibling(x,y) <- hasChild(z, x), hasChild(z, y), x =/= y
* hasSibling(x,y) <- hasSibling(y,x)
* hasSpouse(x,y) <- hasSpouse(y,x)

To generate the cardinalities we follow the rules on the complete graph:
* num(hasBirthPlace, x) = 1 if type(x, human)
* num(hasFather, x) = 1 if type(x, human)
* num(hasMother, x) = 1 if type(x, human)
* num(hasDeathPlace, x) = 1 if exists y: hasDeathDate(x, y) or hasDeathPlace(x, y)
* num(p, x) = #y: p(x, y) if exists y: p(x, y) and p is in {hasChild, hasSibling, hasSpouse and hasStepParent}

Then you can run the experimentation with
`python3 patterns_using_cardinalities_people.py ../eval/wikidata/wikidata_people_triples_full.tsv ../eval/wikidata/wikidata_people_cardinalities.tsv output_file.tsv`
This should be done in the `carl` directory.


## Cardinality mining

To filter and simplify triples related ot people you should run:
```
python3 build_wikidata_people wikidata-YYYYMMDD-truthy-BETA.nt.gz wikidata_people_triples.tsv
```

To create file with only cardinalities related to people:
```
grep -P 'P(22|25|26|40|3373|3448|19|20|27)\t' cardinalities.tsv > cardinalities_people.tsv
```

Then you can run the experimentation with:
```
python3 cardinalities_mining.py ../eval/wikidata/input_triples.tsv ../eval/wikidata/cardinalities_people.tsv output_file.tsv
```
This should be done in the `carl` directory.

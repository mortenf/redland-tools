# redland-tools

A collection of command line scripts for Redland (librdf).

## nt-rdf

Convert NTriples to RDF/XML.

## rdf-digest

Create digest ID(s) for Redland MySQL storage.

## rdf-load

Load statements from URI and store in persistent Redland storage.

## rdf-smush

Smush a persistent Redland storage by owl:IFP's and owl:FP's.

## rdf-tree

Retrieve statements from persistent Redland storage.

# Tests

Run `./test.sh` to build the tools and test them against a BDB ('hashes') storage
in the current directory. The storage uses model name `redland-tools-test`, and
its `redland-tools-test-*.db` files are removed before and after the run. The
script prints PASS/FAIL per step and exits non-zero if any step fails.

The test data is `doap.rdf`, a DOAP description whose project node has
`doap:name` "Redland Tools", and `test.rdf`, a separate blank node with
`dc:title` "Redland Tools". Both nodes have the same `foaf:isPrimaryTopicOf`
value (https://github.com/mortenf/redland-tools).

* rdf-digest: the digest of https://github.com/mortenf/redland-tools is
  `15546633829011637680` (on little-endian hosts).
* rdf-load: `doap.rdf` is loaded into a new storage (`-n`), then `test.rdf` is
  added, each in its own context.
* rdf-tree (before smushing): the N-Triples output has `dc:title` and
  `doap:name` "Redland Tools" on two different nodes.
* rdf-smush: smushing on `foaf:isPrimaryTopicOf` (`-i`, an
  owl:InverseFunctionalProperty) succeeds.
* rdf-tree (after smushing): the output contains a single node with both
  `dc:title` and `doap:name` "Redland Tools".

# License

See [https://github.com/dajobe/librdf](https://github.com/dajobe/librdf).

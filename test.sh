#!/bin/sh
#
# test.sh - Test redland-tools using a BDB ('hashes') storage in the current directory.
#
set -u

cd "$(dirname "$0")" || exit 1

MODEL=redland-tools-test
DIR=./
REPO=https://github.com/mortenf/redland-tools
DIGEST_EXPECTED=15546633829011637680
DC_TITLE='<http://purl.org/dc/elements/1.1/title> "Redland Tools"'
DOAP_NAME='<http://usefulinc.com/ns/doap#name> "Redland Tools"'
IFP=http://xmlns.com/foaf/0.1/isPrimaryTopicOf

failures=0

pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; failures=$((failures+1)); }

cleanup() { rm -f "$DIR$MODEL"-*.db; }
trap cleanup EXIT
cleanup

# Prints the distinct subjects of N-Triples on stdin matching the given predicate/object.
subjects() { grep -F "$1" | cut -d' ' -f1 | sort -u; }

make || exit 1

# rdf-digest
digest=$(./rdf-digest "$REPO")
if [ "$digest" = "$DIGEST_EXPECTED" ]; then
	pass "rdf-digest $REPO = $digest"
else
	fail "rdf-digest $REPO = '$digest', expected $DIGEST_EXPECTED"
fi

# rdf-load
if ./rdf-load -q -n -d "$DIR" -m "$MODEL" "file://$PWD/doap.rdf" \
		&& ./rdf-load -q -d "$DIR" -m "$MODEL" "file://$PWD/test.rdf"; then
	pass "rdf-load doap.rdf and test.rdf"
else
	fail "rdf-load doap.rdf and test.rdf"
fi

# rdf-tree before smushing: dc:title and doap:name are on two different nodes.
before=$(./rdf-tree -q -o ntriples -d "$DIR" -m "$MODEL")
title_nodes=$(echo "$before" | subjects "$DC_TITLE")
name_nodes=$(echo "$before" | subjects "$DOAP_NAME")
if [ -n "$title_nodes" ] && [ -n "$name_nodes" ] && [ "$title_nodes" != "$name_nodes" ]; then
	pass "rdf-tree before smush: dc:title and doap:name on separate nodes"
else
	fail "rdf-tree before smush: expected dc:title and doap:name on separate nodes"
fi

# rdf-smush on foaf:isPrimaryTopicOf
if ./rdf-smush -q -d "$DIR" -m "$MODEL" -i "$IFP"; then
	pass "rdf-smush -i $IFP"
else
	fail "rdf-smush -i $IFP"
fi

# rdf-tree after smushing: a single node has both dc:title and doap:name.
after=$(./rdf-tree -q -o ntriples -d "$DIR" -m "$MODEL")
title_nodes=$(echo "$after" | subjects "$DC_TITLE")
name_nodes=$(echo "$after" | subjects "$DOAP_NAME")
if [ -n "$title_nodes" ] && [ "$(echo "$title_nodes" | wc -l)" -eq 1 ] && [ "$title_nodes" = "$name_nodes" ]; then
	pass "rdf-tree after smush: single node $title_nodes with dc:title and doap:name"
else
	fail "rdf-tree after smush: expected a single node with dc:title and doap:name"
	echo "$after"
fi

if [ "$failures" -eq 0 ]; then
	echo "All tests passed."
else
	echo "$failures test(s) failed."
	exit 1
fi

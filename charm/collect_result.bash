#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/

OUT_DIR=./examples/charm++/state_space_searchengine/UnbalancedTreeSearch_SE/out

./a2sql/bin/txt2sql result.db \
  --drop --table charm_uts \
  -e 'Charm\+\+> Running in non-SMP mode: *(?P<np>\d+) *processes' \
  -r '\[(?P<i>\d+)\] *(?P<time>\d+) *ns *(?P<throughput>[-+]?(\d+(\.\d*)?|\.\d+)([eE][-+]?\d+)?) *Gnodes/s *\( *nodes: *(?P<nodes>\d+) *\)' \
  ${OUT_DIR}/*.txt

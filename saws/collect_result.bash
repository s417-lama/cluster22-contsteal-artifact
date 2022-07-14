#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/

OUT_DIR=./examples/uts/out

./a2sql/bin/txt2sql result.db \
  --drop --table saws_uts \
  -e '# of processes: *(?P<np>\d+)' \
  -r '\[(?P<i>\d+)\] *(?P<time>\d+) *ns *(?P<throughput>[-+]?(\d+(\.\d*)?|\.\d+)([eE][-+]?\d+)?) *Gnodes/s *\( *nodes: *(?P<nodes>\d+) *depth: *(?P<depth>\d+) *leaves: *(?P<leaves>\d+) *\)' \
  ${OUT_DIR}/*.txt

#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/

OUT_DIR=./GLB-UTS/out

./a2sql/bin/txt2sql result.db \
  --drop --table x10_uts \
  -e 'places=(?P<np>\d+)' \
  -r '\[(?P<i>\d+)\] *(?P<time>\d+) *ns *(?P<throughput>[-+]?(\d+(\.\d*)?|\.\d+)([eE][-+]?\d+)?) *Gnodes/s *\( *nodes: *(?P<nodes>\d+) *\)' \
  ${OUT_DIR}/*.txt

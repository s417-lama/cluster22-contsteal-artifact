#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

NODES=${1:-1}

cd -P $(dirname $0)/..

pjsub \
  --interact \
  --sparam wait-time=unlimited \
  -x MACHINE_NAME=obcx \
  -g gc64 \
  -L rscgrp=interactive,node=$NODES

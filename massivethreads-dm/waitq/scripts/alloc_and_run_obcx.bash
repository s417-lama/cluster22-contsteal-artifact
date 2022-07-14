#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

NODES=$1

cd -P $(dirname $0)/..

COMMANDS=()
whitespace="[[:space:]]"
for i in "${@:2}"; do
  if [[ $i =~ $whitespace ]]; then
    i=\\\"$i\\\"
  fi
  COMMANDS+=("$i")
done

JOB_COMMANDS="
  source scripts/envs.bash
  "${COMMANDS[@]}"
"

isola create -o test bash -c "
  echo \"$JOB_COMMANDS\" | pjsub \
    --interact \
    --sparam wait-time=unlimited \
    -g gc64 \
    -L rscgrp=interactive,node=$NODES
"

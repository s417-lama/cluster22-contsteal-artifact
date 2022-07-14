#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..
source scripts/envs.bash

PROJECT=dnpbenchs

# Ugly hack to link 'latest' to the currently running isola
# This is needed because 'latest' is updated when `isola create` finishes
isola cleanup

TARGET_RANK=${1:-0}
COMM_FILE=${COMM_FILE:-"$(isola where ${PROJECT}:test)/${PROJECT}/gdbserver_comm_${TARGET_RANK}.txt"}
COMM=$(cat $COMM_FILE)

echo $COMM

gdb -ex "set pagination off" -ex "target extended-remote $COMM"

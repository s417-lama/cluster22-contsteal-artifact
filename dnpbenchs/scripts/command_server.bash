#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..
source scripts/envs.bash

COMMANDS_FILE=results/commands.ignore

mkdir -p results

touch $COMMANDS_FILE
> $COMMANDS_FILE

export PS4="\n+ "
tail -f $COMMANDS_FILE | stdbuf -oL bash -x

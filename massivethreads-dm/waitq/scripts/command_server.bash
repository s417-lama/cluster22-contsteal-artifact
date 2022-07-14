#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..

COMMANDS_FILE=results/commands.ignore

touch $COMMANDS_FILE
> $COMMANDS_FILE

tail -f $COMMANDS_FILE | stdbuf -oL bash

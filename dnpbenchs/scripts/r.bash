#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..

COMMANDS_FILE=results/commands.ignore

COMMANDS=()
whitespace="[[:space:]]"
for i in "$@"; do
  if [[ $i =~ $whitespace ]]; then
    i=\"$i\"
  fi
  COMMANDS+=("$i")
done

echo -e "${COMMANDS[@]}" >> $COMMANDS_FILE

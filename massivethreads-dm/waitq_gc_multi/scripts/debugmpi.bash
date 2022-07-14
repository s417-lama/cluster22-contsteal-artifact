#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..

TARGET_RANK=${TARGET_RANK:-0}
RANK=${OMPI_COMM_WORLD_RANK:-${PMI_RANK:--1}}

if [[ $RANK == $TARGET_RANK ]]; then
  if nvr --nostart -s; then
    # Vim is running in the current dir

    # Find an available port
    while
      PORT=$(cat /proc/sys/net/ipv4/ip_local_port_range | sed -e 's/[ \t][ \t]*/-/' | xargs -I RANGE shuf -i RANGE -n 1)
      nc -z localhost $PORT
    do :; done

    # Start gdb session in Vim
    COMMANDS=":Termdebug<cr>aset args "${@:2}"<cr>target extended-remote :$PORT<cr>"
    nvr --nostart -s --remote-send "$COMMANDS"
    sleep 1

    # Launch gdbserver
    SHELL=/bin/bash gdbserver --once :$PORT "$@"
  else
    gdb --arg "$@"
  fi
else
  # "$@"
  trun gdb --arg "$@"
fi

echo rank $RANK exits

#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..
source scripts/envs.bash

TARGET_RANKS=${TARGET_RANKS:-0}
MY_RANK=${OMPI_COMM_WORLD_RANK:-${PMI_RANK:--1}}
COMM_FILE=${COMM_FILE:-gdbserver_comm_${MY_RANK}.txt}
USE_VIM=${USE_VIM:-true}
LAUNCH_GDBSERVER=${LAUNCH_GDBSERVER:-true}

find_available_port() {
  while
    port=$(cat /proc/sys/net/ipv4/ip_local_port_range | sed -e 's/[ \t][ \t]*/-/' | xargs -I RANGE shuf -i RANGE -n 1)
    nc -z localhost $port
  do :; done
  echo $port
}

RUN_GDB=false
for i in $TARGET_RANKS; do
  if [[ $MY_RANK == $i ]]; then RUN_GDB=true; fi
done

if $RUN_GDB; then
  if [[ $MACHINE_NAME == local ]]; then
    if $USE_VIM && nvr --nostart -s; then
      # Vim is running in the current dir

      PORT=$(find_available_port)

      # Start gdb session in Vim
      COMMANDS=":Termdebug<cr>aset args "${@:2}"<cr>target extended-remote :$PORT<cr>"
      nvr --nostart -s --remote-send "$COMMANDS"
      sleep 1

      # Launch gdbserver
      SHELL=/bin/bash gdbserver --once :$PORT "$@"
    else
      trun gdb --arg "$@"
    fi
  else
    if $LAUNCH_GDBSERVER; then
      PORT=$(find_available_port)
      echo $(hostname):$PORT > $COMM_FILE
      gdbserver --once :$PORT "$@"
    else
      gdb --arg "$@"
    fi
  fi
else
  "$@"
fi

echo rank $MY_RANK exits

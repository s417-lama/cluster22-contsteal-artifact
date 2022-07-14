#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..

PROJECT=massivethreads-dm

HOST=$1
COMM_FILE=${2:-"\$(isola where ${PROJECT}:test)/${PROJECT}/gdbserver_comm.txt"}
# COMM_FILE=${2:-"~/${PROJECT}/gdbserver_comm.txt"}

# Ugly hack to link 'latest' to the currently running isola
# This is needed because 'latest' is updated when `isola create` finishes
ssh $HOST isola cleanup

COMM=$(ssh $HOST cat $COMM_FILE)
LOCAL_PORT=$(cat /proc/sys/net/ipv4/ip_local_port_range | sed -e 's/[ \t][ \t]*/-/' | xargs -I RANGE shuf -i RANGE -n 1)

echo $COMM

ssh $HOST -O forward -L $LOCAL_PORT:$COMM

GDB_COMMAND="target extended-remote :$LOCAL_PORT"

if !(nvr --nostart -s --remote-send ":Termdebug<cr>a$GDB_COMMAND<cr>"); then
  gdb -ex "$GDB_COMMAND"
  ssh $HOST -O cancel -L $LOCAL_PORT:$COMM
fi

# FIXME: How to cancel SSH forwarding in case of vim?

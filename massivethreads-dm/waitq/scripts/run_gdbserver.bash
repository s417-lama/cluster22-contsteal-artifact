#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..

COMM_FILE=${COMM_FILE:-gdbserver_comm.txt}
PORT=$(cat /proc/sys/net/ipv4/ip_local_port_range | sed -e 's/[ \t][ \t]*/-/' | xargs -I RANGE shuf -i RANGE -n 1)

echo $(hostname):$PORT > $COMM_FILE

gdbserver --once :$PORT "$@"

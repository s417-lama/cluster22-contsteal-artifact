#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/

EXEC_TIME=${EXEC_TIME:-1:00:00}
# EXEC_TIME=${EXEC_TIME:-3:00:00}

NODES=${NODES:-1}

if (( $NODES == 1 )); then
  rscgrp=ito-ss
elif (( $NODES <= 4 )); then
  rscgrp=ito-s
elif (( $NODES <= 16 )); then
  rscgrp=ito-m
elif (( $NODES <= 64 )); then
  rscgrp=ito-l
elif (( $NODES <= 128 )); then
  rscgrp=ito-xl
elif (( $NODES <= 256 )); then
  rscgrp=ito-xxl
else
  echo "Allocating $NODES nodes is not allowed on ITO."
  exit 1
fi

jobid=$(pjsub -X -j -L rscunit=ito-a,rscgrp=$rscgrp,vnode=$NODES,vnode-core=36,elapse=$EXEC_TIME ./run_uts.bash |
  grep submitted | cut -d ' ' -f 6)

echo Job $jobid submitted.

while pjstat $jobid | grep $jobid | grep QUE > /dev/null; do
  sleep 1
done

echo Job $jobid started.

start_time=$(date +%s)

while pjstat $jobid | grep $jobid > /dev/null; do
  sleep 1
done

end_time=$(date +%s)

echo Job $jobid completed in $(date -d@$((end_time - start_time)) -u +%H:%M:%S).

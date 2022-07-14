#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

MACHINE_NAME=$1
JOB_SCRIPT=$2

cd -P $(dirname $0)/..

source $JOB_SCRIPT

JOB_NAME=$(dnp_job_name)
MADM_ISOLA=$(dnp_madm_isola)
MAX_NODES=$(dnp_max_nodes)

JOB_COMMANDS="#!/bin/bash
  source scripts/envs.bash
  source $JOB_SCRIPT
  dnp_print_env
  dnp_run
"

case $MACHINE_NAME in
  ito)
    EXEC_TIME=${EXEC_TIME:-1:00:00}
    # EXEC_TIME=${EXEC_TIME:-3:00:00}
    # EXEC_TIME=${EXEC_TIME:-4:00:00}

    if (( $MAX_NODES == 1 )); then
      rscgrp=ito-ss
    elif (( $MAX_NODES <= 4 )); then
      rscgrp=ito-s
    elif (( $MAX_NODES <= 16 )); then
      rscgrp=ito-m
    elif (( $MAX_NODES <= 64 )); then
      rscgrp=ito-l
    elif (( $MAX_NODES <= 128 )); then
      rscgrp=ito-xl
    elif (( $MAX_NODES <= 256 )); then
      rscgrp=ito-xxl
    else
      echo "Allocating $MAX_NODES nodes is not allowed on ITO."
      exit 1
    fi

    isola create -b $MADM_ISOLA $JOB_NAME bash -c "
      jobid=\$(echo \"$JOB_COMMANDS\" |
        pjsub -X -j -L rscunit=ito-a,rscgrp=$rscgrp,vnode=$MAX_NODES,vnode-core=36,elapse=$EXEC_TIME |
        grep submitted | cut -d ' ' -f 6)

      echo Job \$jobid submitted.

      while pjstat \$jobid | grep \$jobid | grep QUE > /dev/null; do
        sleep 1
      done

      echo Job \$jobid started.

      start_time=\$(date +%s)

      while [[ ! -f STDIN.o\${jobid} ]]; do
        sleep 1
      done

      tail -f STDIN.o\${jobid} & pid=\$!

      while pjstat \$jobid | grep \$jobid > /dev/null; do
        sleep 1
      done

      end_time=\$(date +%s)

      kill \$pid

      echo Job \$jobid completed in \$(date -d@\$((end_time - start_time)) -u +%H:%M:%S).
    "
    ;;
  wisteria-o)
    # EXEC_TIME=${EXEC_TIME:-1:00:00}
    # EXEC_TIME=${EXEC_TIME:-1:30:00}
    EXEC_TIME=${EXEC_TIME:-3:00:00}
    n_nodes=$(echo $MAX_NODES | cut -f 1 -d ":" | sed 's/x/*/g' | bc)

    isola create -b $MADM_ISOLA $JOB_NAME bash -c "
      jobid=\$(echo \"$JOB_COMMANDS\" |
        pjsub -X -j -s -S -g gc64 -L rscgrp=regular-o,node=$MAX_NODES,elapse=$EXEC_TIME --mpi proc=$((n_nodes * 48)) |
        grep submitted | cut -d ' ' -f 6)

      echo Job \$jobid submitted.

      while pjstat \$jobid | grep \$jobid | grep QUEUED > /dev/null; do
        sleep 1
      done

      echo Job \$jobid started.

      start_time=\$(date +%s)

      while [[ ! -f STDIN.\${jobid}.out ]]; do
        sleep 1
      done

      tail -f STDIN.\${jobid}.out & pid=\$!

      while pjstat \$jobid | grep \$jobid > /dev/null; do
        sleep 1
      done

      end_time=\$(date +%s)

      kill \$pid

      echo Job \$jobid completed in \$(date -d@\$((end_time - start_time)) -u +%H:%M:%S).
    "
    ;;
  fugaku)
    EXEC_TIME=${EXEC_TIME:-0:06:00}
    # EXEC_TIME=${EXEC_TIME:-1:00:00}
    # EXEC_TIME=${EXEC_TIME:-1:30:00}
    # EXEC_TIME=${EXEC_TIME:-3:00:00}
    n_nodes=$(echo $MAX_NODES | cut -f 1 -d ":" | sed 's/x/*/g' | bc)

    if (( $n_nodes <= 384 )); then
      rscgrp=small
    elif (( $n_nodes <= 55297 )); then
      rscgrp=large
    elif (( $n_nodes <= 82944 )); then
      rscgrp=huge
    else
      echo "Allocating $n_nodes is not allowed on Fugaku."
      exit 1
    fi

    isola create -b $MADM_ISOLA $JOB_NAME bash -c "
      # to prevent env vals are propagated to compute nodes with -X option
      # otherwise mpirun command is not found
      module unload lang/tcsds-1.2.35

      jobid=\$(echo \"$JOB_COMMANDS\" |
        pjsub -N job -X -j -s -S -L rscgrp=$rscgrp,node=$MAX_NODES,elapse=$EXEC_TIME --mpi proc=$((n_nodes * 48)) |
        grep submitted | cut -d ' ' -f 6)

      echo Job \$jobid submitted.

      while pjstat \$jobid | grep \$jobid | grep -e QUE -e RNA > /dev/null; do
        sleep 1
      done

      echo Job \$jobid started.

      start_time=\$(date +%s)

      while [[ ! -f job.\${jobid}.out ]]; do
        sleep 1
      done

      tail -f job.\${jobid}.out & pid=\$!

      while pjstat \$jobid | grep \$jobid > /dev/null; do
        sleep 1
      done

      end_time=\$(date +%s)

      kill \$pid

      echo Job \$jobid completed in \$(date -d@\$((end_time - start_time)) -u +%H:%M:%S).
    "
    ;;
  *)
    echo "Unknown machine name '$MACHINE_NAME'"
    exit 1
    ;;
esac

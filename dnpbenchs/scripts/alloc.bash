#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

MACHINE_NAME=$1
NODES=${2:-1}

OUT_FILE=results/out.ignore

cd -P $(dirname $0)/..

case $MACHINE_NAME in
  obcx)
    pjsub \
      --interact \
      --sparam wait-time=unlimited \
      -g gc64 \
      -L rscgrp=interactive,node=$NODES
    ;;
  ito)
    # elapse_time=1:00:00
    elapse_time=6:00:00

    mail_options=""
    # mail_options="-m b --mail-list shiina@eidos.ic.i.u-tokyo.ac.jp"

    delete_on_exit=true
    # delete_on_exit=false

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

    dirty_str=$(uuidgen)
    mkdir -p results
    echo $dirty_str > $OUT_FILE

    # Configure sshd: https://sourceware.org/legacy-ml/cygwin/2008-04/msg00363.html
    jobid=$(echo "hostname > $OUT_FILE &&
                  printenv | grep -e PJM -e PLE_SCRIPT_TYPE > ~/.ssh/environment &&
                  /usr/sbin/sshd -f $HOME/etc/sshd_config -D" |
      pjsub -j $mail_options -L rscunit=ito-a,rscgrp=$rscgrp,vnode=$NODES,vnode-core=36,elapse=$elapse_time |
      grep submitted | cut -d ' ' -f 6)

    if $delete_on_exit; then
      trap "pjdel $jobid" EXIT
    fi

    echo Job $jobid submitted.

    while pjstat $jobid | grep $jobid | grep QUE > /dev/null; do
      sleep 1
    done

    echo Job $jobid started.

    while cat $OUT_FILE | grep $dirty_str > /dev/null; do
      sleep 1
    done

    HOST=$(cat $OUT_FILE)
    PORT=2022

    while ! nc -z $HOST $PORT; do
      sleep 1
    done

    export ITO_ALLOC_NODES=$NODES
    ssh -p $PORT $HOST
    ;;
  wisteria-o)
    n_nodes=$(echo $NODES | cut -f 1 -d ":" | sed 's/x/*/g' | bc)
    pjsub \
      --interact \
      --sparam wait-time=unlimited \
      -j -o jobout.o \
      -g gc64 \
      -L rscgrp=interactive-o,node=$NODES \
      --mpi proc=$((n_nodes * 48))
    ;;
  fugaku)
    n_nodes=$(echo $NODES | cut -f 1 -d ":" | sed 's/x/*/g' | bc)
    pjsub \
      --interact \
      --sparam wait-time=unlimited \
      -L node=$NODES,elapse=0:30:00 \
      --mpi proc=$((n_nodes * 48))
    ;;
  *)
    echo "Unknown machine name '$MACHINE_NAME'"
    exit 1
    ;;
esac

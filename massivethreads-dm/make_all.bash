#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)

VERSIONS="waitq waitq_gc greedy greedy_gc child_stealing child_stealing_ult future_multi waitq_gc_multi child_stealing_ult_multi"

for version in $VERSIONS; do
  cp -f make_common.bash ../envs.bash $version
  (
    export LOGGER_TYPE=none
    cd $version
    isola create -o -p massivethreads-dm $version bash ./make_common.bash
  )
done

PROF_VERSIONS="waitq waitq_gc greedy_gc child_stealing child_stealing_ult"

for version in $PROF_VERSIONS; do
  cp -f make_common.bash ../envs.bash $version
  (
    export LOGGER_TYPE=stats
    cd $version
    isola create -o -p massivethreads-dm ${version}_prof bash ./make_common.bash
  )
done

TRACE_VERSIONS="waitq_gc greedy_gc child_stealing_ult"

for version in $TRACE_VERSIONS; do
  cp -f make_common.bash ../envs.bash $version
  (
    export LOGGER_TYPE=trace
    cd $version
    isola create -o -p massivethreads-dm ${version}_trace bash ./make_common.bash
  )
done

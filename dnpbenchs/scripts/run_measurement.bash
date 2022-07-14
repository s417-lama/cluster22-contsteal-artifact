#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..

MACHINE_NAME=$1
JOB_SCRIPT=$2

source $JOB_SCRIPT

JOB_NAME=$(dnp_job_name)
MADM_ISOLA=$(dnp_madm_isola)

isola create -b $MADM_ISOLA $JOB_NAME bash -c "source scripts/envs.bash && source $JOB_SCRIPT && dnp_print_env && dnp_run"

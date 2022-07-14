#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..

JOB_SCRIPT=$1

source $JOB_SCRIPT

JOB_NAME=$(dnp_job_name)
MADM_ISOLA=$(dnp_madm_isola)

COMMANDS_FILE=results/commands.ignore

JOB_COMMANDS="
(
  source scripts/envs.bash

  isola create -b $MADM_ISOLA $JOB_NAME bash -c \"source $JOB_SCRIPT && dnp_print_env && dnp_run\"
)
"

echo -e "$JOB_COMMANDS" >> $COMMANDS_FILE

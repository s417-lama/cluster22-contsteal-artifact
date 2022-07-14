#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/

export REPEATS=3
# export REPEATS=100

# -------------------- 1 core --------------------

# T1L
(
  export NODES=1
  export CORES=1
  export UTS_TREE=T1L
  export WARMUPS=10
  export EXEC_TIME=1:30:00
  ./submit.bash
) &

# -------------------- 1 node --------------------

# T1L
(
  export NODES=1
  export CORES=36
  export UTS_TREE=T1L
  export WARMUPS=10
  export EXEC_TIME=0:30:00
  ./submit.bash
) &

# -------------------- 4 nodes --------------------

# T1L
(
  export NODES=4
  export CORES=36
  export UTS_TREE=T1L
  export WARMUPS=100
  export EXEC_TIME=0:30:00
  ./submit.bash
) &

# T1XXL
(
  export NODES=4
  export CORES=36
  export UTS_TREE=T1XXL
  export WARMUPS=10
  export EXEC_TIME=0:30:00
  ./submit.bash
) &

# -------------------- 16 nodes --------------------

# T1L
(
  export NODES=16
  export CORES=36
  export UTS_TREE=T1L
  export WARMUPS=1000
  export EXEC_TIME=0:30:00
  ./submit.bash
) &

# T1XXL
(
  export NODES=16
  export CORES=36
  export UTS_TREE=T1XXL
  export WARMUPS=100
  export EXEC_TIME=0:30:00
  ./submit.bash
) &

# -------------------- 64 nodes --------------------

# T1L
(
  export NODES=64
  export CORES=36
  export UTS_TREE=T1L
  export WARMUPS=10000
  export EXEC_TIME=0:30:00
  ./submit.bash
) &

# T1XXL
(
  export NODES=64
  export CORES=36
  export UTS_TREE=T1XXL
  export WARMUPS=1000
  export EXEC_TIME=0:30:00
  ./submit.bash
) &

# T1WL
(
  export NODES=64
  export CORES=36
  export UTS_TREE=T1WL
  export WARMUPS=100
  export EXEC_TIME=2:00:00
  ./submit.bash
) &

# -------------------- 256 nodes --------------------

# T1L
(
  export NODES=256
  export CORES=36
  export UTS_TREE=T1L
  export WARMUPS=100000
  export EXEC_TIME=1:00:00
  ./submit.bash
) &

# T1XXL
(
  export NODES=256
  export CORES=36
  export UTS_TREE=T1XXL
  export WARMUPS=10000
  export EXEC_TIME=1:00:00
  ./submit.bash
) &

# T1WL
(
  export NODES=256
  export CORES=36
  export UTS_TREE=T1WL
  export WARMUPS=100
  export EXEC_TIME=1:00:00
  ./submit.bash
) &

wait

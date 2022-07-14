#!/bin/bash

source ../massivethreads-dm/envs.bash

log2() {
  local x=0
  for ((y = $1 - 1; y > 0; y >>= 1)); do
    let x=$((x + 1))
  done
  echo $x
}
export -f log2

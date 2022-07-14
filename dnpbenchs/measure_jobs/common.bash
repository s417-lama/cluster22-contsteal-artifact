#!/bin/bash

dnp_run_common() {
  case "$MACHINE_NAME" in
    ito)
      DNP_CORES=36
      DNP_NODES=$PJM_VNODES
      dnp_mpirun() {
        local n_processes=$1
        local n_processes_per_node=$2
        local out_file=$3
        mpirun -n $n_processes -N $n_processes_per_node \
          --mca plm_rsh_agent pjrsh \
          --hostfile $PJM_O_NODEINF \
          --mca osc_ucx_acc_single_intrinsic true \
          --bind-to core \
          ../opt/massivethreads-dm/bin/madm_disable_aslr "${@:4}" | tee $out_file
      }
      ;;
    wisteria-o | fugaku)
      DNP_CORES=48
      DNP_NODES=$PJM_NODE
      dnp_mpirun() {
        local n_processes=$1
        local n_processes_per_node=$2
        local out_file=$3
        mpirun -n $n_processes \
          --vcoordfile <(
            np=0
            if [[ -z ${PJM_NODE_Y+x} ]]; then
              # 1D
              for x in $(seq 1 $PJM_NODE_X); do
                for i in $(seq 1 $n_processes_per_node); do
                  echo "($((x-1)))"
                  if (( ++np >= n_processes )); then
                    break
                  fi
                done
              done
            elif [[ -z ${PJM_NODE_Z+x} ]]; then
              # 2D
              for x in $(seq 1 $PJM_NODE_X); do
                for y in $(seq 1 $PJM_NODE_Y); do
                  for i in $(seq 1 $n_processes_per_node); do
                    echo "($((x-1)),$((y-1)))"
                    if (( ++np >= n_processes )); then
                      break 2
                    fi
                  done
                done
              done
            else
              # 3D
              for x in $(seq 1 $PJM_NODE_X); do
                for y in $(seq 1 $PJM_NODE_Y); do
                  for z in $(seq 1 $PJM_NODE_Z); do
                    for i in $(seq 1 $n_processes_per_node); do
                      echo "($((x-1)),$((y-1)),$((z-1)))"
                      if (( ++np >= n_processes )); then
                        break 3
                      fi
                    done
                  done
                done
              done
            fi
          ) \
          --std $out_file \
          ../opt/massivethreads-dm/bin/madm_disable_aslr "${@:4}"
        cat $out_file
      }
      ;;
    *)
      echo "Error: \$MACHINE_NAME=$MACHINE_NAME is invalid." >&2; exit 1 ;;
  esac

  n_nodes=$(echo $DNP_MAX_NODES | cut -f 1 -d ":" | sed 's/x/*/g' | bc)
  if [[ $DNP_NODES < $n_nodes ]]; then
    echo "Error: $n_nodes nodes are required but only $DNP_NODES nodes are allocated" >&2
    exit 1
  fi

  export MADM_RUN__=1
  export MADM_PRINT_ENV=1
}

dnp_job_name() {
  max_nodes=$(echo $DNP_MAX_NODES | sed 's/:/-/g')
  echo ${DNP_PREFIX}_madm_${DNP_MADM_BUILD}_node_${max_nodes}
}

dnp_max_nodes() {
  echo $DNP_MAX_NODES
}

dnp_madm_isola() {
  echo massivethreads-dm:${DNP_MADM_BUILD}
}

dnp_print_env() {
  hostname
  date
  case "$MACHINE_NAME" in
    wisteria-o | fugaku) mpiFCC --version ;;
    *)                   mpicxx --version ;;
  esac
  mpirun --version
  echo
}

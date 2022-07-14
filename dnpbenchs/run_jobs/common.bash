source scripts/envs.bash

HOME_DIR=${ISOLA_HOME:-${HOME}}
MACHINE_NAME=${MACHINE_NAME:-local}

case $MACHINE_NAME in
  ito)
    cores=36
    nodes=$PJM_VNODES

    dnp_mpirun() {
      local n_processes=$1
      local n_processes_per_node=$2
      mpirun -n $n_processes -N $n_processes_per_node \
        --mca plm_rsh_agent pjrsh \
        --hostfile $PJM_O_NODEINF \
        --mca osc_ucx_acc_single_intrinsic true \
        --bind-to core \
        ${HOME_DIR}/opt/massivethreads-dm/bin/madm_disable_aslr "${@:3}"
    }

    dnp_print_env() {
      mpicxx --version
      mpirun --version
    }
    ;;
  wisteria-o | fugaku)
    cores=48
    nodes=$PJM_NODE

    dnp_mpirun() {
      local n_processes=$1
      local n_processes_per_node=$2
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
        ${HOME_DIR}/opt/massivethreads-dm/bin/madm_disable_aslr "${@:3}"
    }

    dnp_print_env() {
      mpiFCC --version
      mpirun --version
    }
    ;;
  *)
    cores=6
    nodes=1

    dnp_mpirun() {
      local n_processes=$1
      local n_processes_per_node=$2
      mpirun -n $n_processes -N $n_processes_per_node \
        ${HOME_DIR}/opt/massivethreads-dm/bin/madm_disable_aslr "${@:3}"
    }

    dnp_print_env() {
      mpicxx --version
      mpirun --version
    }

    N=1024

    ;;
esac

dnp_print_env

export MADM_RUN__=1
export MADM_PRINT_ENV=1

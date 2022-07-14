#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

###############################################################################
# Config
###############################################################################

project_root() {
  # git rev-parse --show-toplevel || pwd -P
  pwd -P
}

project_name() {
  basename $(project_root)
}

create_new_tmp_dir() {
  local path=${ISOLA_ROOT}/tmp/$(uuidgen)
  mkdir -p $path
  echo $path
  # mktemp -d
}

###############################################################################
# Utils
###############################################################################

is_simlink() {
  local path=$1
  test -L $path
}

is_valid_simlink() {
  local path=$1
  test -L $path && test -e $path
}

is_broken_simlink() {
  local path=$1
  is_simlink $path && ! is_valid_simlink $path
}

resolve_simlink() {
  local path=$1
  (cd $path && pwd -P)
}

###############################################################################
# Convert Types (Unsafe)
###############################################################################

isola_id2path() {
  local isola_id=$1
  echo ${ISOLA_ROOT}/projects/${isola_id//:/\/}
}

isola_path2id() {
  local path=$1
  local ps=(${path//\// })
  local project=${ps[@]:(-3):1}
  local name=${ps[@]:(-2):1}
  local version=${ps[@]:(-1):1}
  echo ${project}:${name}:${version}
}

isola_id2realid() {
  local isola_id=$1
  local path=$(resolve_simlink $(isola_id2path $isola_id))
  isola_path2id $path
}

isola_tokens2path() {
  if [[ "$#" == 2 ]]; then
    # project:name
    local project=$1
    local name=$2
    echo ${ISOLA_ROOT}/projects/${project}/${name}/latest
  elif [[ "$#" == 3 ]]; then
    # project:name:version
    local project=$1
    local name=$2
    local version=$3
    echo ${ISOLA_ROOT}/projects/${project}/${name}/${version}
  else
    echo "Internal Error." >&2
    exit 1
  fi
}

###############################################################################
# Parse Isola IDs
###############################################################################

validate_project() {
  local project=$1
  if [ ! -e ${ISOLA_ROOT}/projects/${project} ]; then
    echo "No project '$project' exists." >&2
    exit 1
  fi
}

validate_name() {
  local project=$1
  local name=$2
  validate_project $project
  if [ ! -e ${ISOLA_ROOT}/projects/${project}/${name} ]; then
    echo "No name '$name' exists in Isola project '$project'." >&2
    exit 1
  fi
}

validate_version() {
  local project=$1
  local name=$2
  local version=$3
  validate_project $project
  validate_name $project $name
  if [ ! -e ${ISOLA_ROOT}/projects/${project}/${name}/${version} ]; then
    echo "No version '$version' exists in Isola '${project}:${name}'." >&2
    exit 1
  fi
}

parse_isola_id2tokens() {
  local isola_id=$1
  local tokens=(${isola_id//:/ })
  if [[ "${#tokens[@]}" == 1 ]]; then
    # project
    local project="${tokens[0]}"
    validate_project $project
    echo $project
  elif [[ "${#tokens[@]}" == 2 ]]; then
    # project:name
    local project="${tokens[0]}"
    local name="${tokens[1]}"
    validate_name $project $name
    echo $project $name
  elif [[ "${#tokens[@]}" == 3 ]]; then
    # project:name:version
    local project="${tokens[0]}"
    local name="${tokens[1]}"
    local version="${tokens[2]}"
    validate_version $project $name $version
    echo $project $name $version
  else
    echo "Isola Parse Error: isola format '$isola_id' is invalid." >&2
    echo "## Isola format must be 'project[:name[:version]]'" >&2
    exit 1
  fi
}

parse_isola_id2path() {
  local isola_id=$1
  local tokens=($(parse_isola_id2tokens $isola_id))
  [[ -z "${tokens[@]}" ]] && exit 1
  if [[ "${#tokens[@]}" == 2 || "${#tokens[@]}" == 3 ]]; then
    isola_tokens2path "${tokens[@]}"
  else
    echo "Isola '$isola_id' must include name." >&2
    echo "## Isola format must be 'project:name[:version]'" >&2
    exit 1
  fi
}

parse_isola_id2realpath() {
  local isola_id=$1
  local isola_path=$(parse_isola_id2path $isola_id)
  [[ -z "$isola_path" ]] && exit 1
  resolve_simlink $isola_path
}

###############################################################################
# List Management
###############################################################################

get_project_list() {
  find ${ISOLA_ROOT}/projects -mindepth 1 -maxdepth 1 -printf "%f\n" | sort
}

get_name_list() {
  local project=$1
  find ${ISOLA_ROOT}/projects/${project} -mindepth 1 -maxdepth 1 -printf "${project}:%f\n" | sort
}

get_version_list() {
  local project=$1
  local name=$2
  find ${ISOLA_ROOT}/projects/${project}/${name} -mindepth 1 -maxdepth 1 -printf "${project}:${name}:%f\n" | sort
}

get_version_list_wo_simlink() {
  local project=$1
  local name=$2
  find ${ISOLA_ROOT}/projects/${project}/${name} -mindepth 1 -maxdepth 1 -type d -printf "${project}:${name}:%f\n" | sort
}

get_isola_list() {
  if [[ "$#" == 0 ]]; then
    get_project_list
  else
    local isola_id=$1
    local tokens=($(parse_isola_id2tokens $isola_id))
    [[ -z "${tokens[@]}" ]] && exit 1
    if [[ "${#tokens[@]}" == 1 ]]; then
      # project
      get_name_list "${tokens[@]}"
    elif [[ "${#tokens[@]}" == 2 ]]; then
      # project:name
      get_version_list "${tokens[@]}"
    elif [[ "${#tokens[@]}" == 3 ]]; then
      # project:name:version
      echo "$(IFS=':'; echo "${tokens[*]}")"
    else
      echo "Internal Error." >&2
      exit 1
    fi
  fi
}

get_rec_project_list() {
  for project in $(get_project_list); do
    get_rec_name_list $project
  done
}

get_rec_name_list() {
  local project=$1
  for name in $(get_name_list $project | cut -d ':' -f 2); do
    get_version_list $project $name
  done
}

get_rec_isola_list() {
  if [[ "$#" == 0 ]]; then
    get_rec_project_list
  else
    local isola_id=$1
    local tokens=($(parse_isola_id2tokens $isola_id))
    [[ -z "${tokens[@]}" ]] && exit 1
    if [[ "${#tokens[@]}" == 1 ]]; then
      # project
      get_rec_name_list "${tokens[@]}"
    elif [[ "${#tokens[@]}" == 2 ]]; then
      # project:name
      get_version_list "${tokens[@]}"
    elif [[ "${#tokens[@]}" == 3 ]]; then
      # project:name:version
      echo "$(IFS=':'; echo "${tokens[*]}")"
    else
      echo "Internal Error." >&2
      exit 1
    fi
  fi
}

###############################################################################
# Cleanup
###############################################################################

remove_isola() {
  local isola_id=$1
  local path=$(isola_id2path $isola_id)
  echo -e "Removing ${isola_id}..." 2>&1
  chmod +w $path
  rm -rf $path
}

relink_latest_simlink() {
  local project=$1
  local name=$2
  local simlink_path=$(isola_id2path ${project}:${name}:latest)
  local isolas=($(get_version_list_wo_simlink $project $name))
  if [[ "${#isolas[@]}" == 0 ]]; then
    # if no available isola is found, just remove this group
    rm -f $simlink_path
  else
    local latest_isola=${isolas[@]:(-1):1}
    local latest_isola_path=$(isola_id2path $latest_isola)
    if is_valid_simlink $simlink_path && [[ $latest_isola_path != $(resolve_simlink $simlink_path) ]]; then
      echo "Relinking ${project}:${name}:latest to $latest_isola..." >&2
    fi
    ln -sfn $latest_isola_path $simlink_path
  fi
}

cleanup_projects() {
  for project in $(get_project_list); do
    for name in $(get_name_list $project | cut -d ':' -f 2); do
      # handle incorrect 'latest' tag
      relink_latest_simlink $project $name
      # if 'name' is empty, remove it
      if [[ -z "$(get_version_list $project $name)" ]]; then
        echo "Removing ${project}:${name}..." >&2
        rm -rf ${ISOLA_ROOT}/projects/${project}/${name}
      fi
    done
    # if 'project' is empty, remove it
    if [[ -z "$(get_name_list $project)" ]]; then
      echo "Removing ${project}..." >&2
      rm -rf ${ISOLA_ROOT}/projects/${project}
    fi
  done
}

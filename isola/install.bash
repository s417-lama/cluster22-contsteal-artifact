#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

ISOLA_ROOT=${ISOLA_ROOT:-${HOME}/.isola}

cd $(dirname $0)
mkdir -p $ISOLA_ROOT
cp -r bin $ISOLA_ROOT
mkdir -p $ISOLA_ROOT/projects

# replace $HOME with ~
[[ "$ISOLA_ROOT" =~ ^"$HOME" ]] && ISOLA_ROOT="~${ISOLA_ROOT#$HOME}"

echo -e "Successfully installed at $ISOLA_ROOT\n"
echo -e "Set PATH variable if needed."
echo -e "Bash:"
echo -e "  echo 'export PATH=${ISOLA_ROOT}/bin:\$PATH' >> ~/.bashrc"
echo -e "Fish:"
echo -e "  echo 'set -gx PATH ${ISOLA_ROOT}/bin \$PATH' >> ~/.config/fish/config.fish"

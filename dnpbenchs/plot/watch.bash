#!/bin/bash
set -u
export LC_ALL=C
export LANG=C

HOST=$1
ROOT_DIR="~/dnpbenchs/results"
SOURCE_PY=$(cat livereload.py)
TTY_FILE=.livereload.tty

ssh $HOST "echo connected to \$(hostname)"

inotifywait -m -e close_write -r . --format "%w%f" --exclude '.*~$' |
  xargs -I FILE bash -c "
    cat FILE | ssh $HOST \"
      cd $ROOT_DIR
      # forward output to 'ssh -t ...' below because the alignment of output breaks
      TARGET_TTY=\\\$(cat $TTY_FILE)
      echo -e \\\"\nrunning FILE on $HOST...\n\\\" > \\\$TARGET_TTY
      python3 - > \\\$TARGET_TTY 2>&1
      echo -e \\\"\ncompleted!\\\" > \\\$TARGET_TTY
    \"
  " &
INOTIFY_PID=$!

EXIT_COMMANDS=$(
  ssh -t $HOST "
    cd $ROOT_DIR
    echo \$SSH_TTY > $TTY_FILE
    REMOTE_PORT=\$(cat /proc/sys/net/ipv4/ip_local_port_range | sed -e 's/[ \t][ \t]*/-/' | xargs -I RANGE shuf -i RANGE -n 1)
    python3 -c '$SOURCE_PY' \$REMOTE_PORT
  " |
    stdbuf -oL tee /dev/stderr |
    stdbuf -oL grep "Serving on" |
    stdbuf -oL cut -d " " -f 7 |
    stdbuf -oL cut -d ":" -f 3 |
    xargs -I REMOTE_PORT bash -c "
      LOCAL_PORT=\$(cat /proc/sys/net/ipv4/ip_local_port_range | sed -e 's/[ \t][ \t]*/-/' | xargs -I RANGE shuf -i RANGE -n 1)
      FORWARD=\$(echo \${LOCAL_PORT}:localhost:REMOTE_PORT | tr -d '\r')
      ssh -o 'ExitOnForwardFailure yes' -O forward -L \$FORWARD $HOST
      echo \"Opening http://localhost:\${LOCAL_PORT} in browser...\" > /dev/stderr
      xdg-open http://localhost:\${LOCAL_PORT} > /dev/null 2>&1
      echo \"ssh -O cancel -L \$FORWARD $HOST\n\"
    "
)

echo "Interrupted."
kill -SIGTERM $INOTIFY_PID
eval "$(echo -e $EXIT_COMMANDS)"

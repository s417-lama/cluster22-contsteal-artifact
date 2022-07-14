#!/bin/bash
export LC_ALL=C
export LANG=C

PROJECT=dnpbenchs

HOST=$1
TRACE_FILE=${2:-"\$(isola where ${PROJECT}:test)/${PROJECT}/mlog.ignore"}
PORT=${PORT:-5006}

ssh -O forward -L $PORT:localhost:$PORT $HOST
ssh $HOST "bokeh serve ~/massivelogger/viewer --port $PORT --check-unused-sessions 1000 --unused-session-lifetime 3000 --args "$TRACE_FILE"" 2>&1 |
    tee /dev/stderr |
    stdbuf -oL grep "Bokeh app running at" |
    xargs -I{} xdg-open http://localhost:$PORT
ssh -O cancel -L $PORT:localhost:$PORT $HOST

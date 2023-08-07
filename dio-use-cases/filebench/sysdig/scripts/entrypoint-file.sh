#!/bin/bash

echo "$(date) | Starting Sysdig..."
echo "$(date) |  -- SYSDIG_COMMAND: ${SYSDIG_COMMAND}"

function save_time(){
    end=`date +%s.%N`
    runtime=$( echo "$end - $start" | bc -l )
    echo "Runtime was $runtime seconds" | tee -a /home/time-sysdig-tracing.txt
}

#--- Stop sysdig
function exit_container(){
    PID=$!
    kill -s SIGINT $PID
    save_time
    exit 0
}

#--- trap the SIGTERM/SIGINT signals
trap exit_container SIGTERM
trap exit_container SIGINT

echo "Starting sysdig..."
start=`date +%s.%N`
eval "${SYSDIG_COMMAND}" -w /home/sysdig_trace.scap &

wait
save_time
#!/bin/bash

echo "$(date) | Starting Sysdig (/dev/null)..."
echo "$(date) |  -- SYSDIG_COMMAND: ${SYSDIG_COMMAND}"

function save_time(){
    end=`date +%s.%N`
    runtime=$( echo "$end - $start" | bc -l )
    echo "Runtime was $runtime seconds" | tee -a /home/time-sysdig-parsing-dev-null.txt
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

echo "Starting sysdig and logstash..."
start=`date +%s.%N`
eval "${SYSDIG_COMMAND}" > /dev/null &

wait
save_time
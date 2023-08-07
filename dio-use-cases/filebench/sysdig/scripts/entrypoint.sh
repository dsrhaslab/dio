#!/bin/bash

echo "$(date) | Starting Sysdig and Logstash..."
echo "$(date) |  -- ES_HOSTS: ${ES_HOSTS}"
echo "$(date) |  -- ES_USERNAME: ${ES_USERNAME}"
echo "$(date) |  -- ES_PASSWORD: ${ES_PASSWORD}"
echo "$(date) |  -- LS_BATCH_SIZE: ${LS_BATCH_SIZE}"
echo "$(date) |  -- LS_BATCH_DELAY: ${LS_BATCH_DELAY}"
echo "$(date) |  -- SYSDIG_COMMAND: ${SYSDIG_COMMAND}"

function save_time(){
    end=`date +%s.%N`
    runtime=$( echo "$end - $start" | bc -l )
    echo "Runtime was $runtime seconds" | tee -a /home/time-sysdig-tracing-and-parsing.txt
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
eval "${SYSDIG_COMMAND}" | /usr/share/logstash/bin/logstash -f /usr/share/logstash/sysdig-logstash.conf &

wait
save_time
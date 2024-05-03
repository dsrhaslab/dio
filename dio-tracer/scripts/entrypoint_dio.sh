#!/bin/bash

set -e
set -m

if ${CORRELATE_PATHS} == "true"; then
    if [[ -z "${ES_SERVERS}" ]]; then
        echo "When using FPCA, you must specify the ES_SERVERS environment variable"
        exit 1
    fi
    echo "Starting FPCA..."
    /usr/share/dio/bin/fpca.sh correlate_daemon $ES_SERVERS $SLEEP_TIME $N_TRIES true > /usr/share/dio/dio_data/fpca.log 2>&1 &
    sleep 1s
fi

echo "Starting DIO..."
/usr/share/dio/bin/dio $DIO_OPTIONS -- ${@} & dio_pid=$!
wait $dio_pid

if ${CORRELATE_PATHS} == "true"; then
    echo "Waiting for FPCA to finish..."
    fpca_pid=$(cat /usr/share/dio/fpca.pid)
    echo $fpca_pid
    wait $fpca_pid
fi
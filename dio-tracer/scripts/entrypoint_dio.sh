#!/bin/bash

set -e
set -m

if ${CORRELATE_PATHS} == "true"; then
    if [[ -z "${ES_SERVERS}" ]]; then
        echo "When using FPCA, you must specify the ES_SERVERS environment variable"
        exit 1
    fi
    echo "Starting FPCA..."
    /usr/share/dio/bin/fpca.sh correlate_fp $ES_SERVERS $SLEEP_TIME $N_TRIES true > /dio_data/fpca.log 2>&1 &
    sleep 1s
fi

echo "Starting DIO..."
exec /usr/share/dio/bin/dio $DIO_OPTIONS -- ${@}

if ${CORRELATE_PATHS} == "true"; then
    echo "Waiting for FPCA to finish..."
    fpca_pid=$(cat /usr/share/dio/fpca.pid)
    echo $fpca_pid
    wait $fpca_pid
fi
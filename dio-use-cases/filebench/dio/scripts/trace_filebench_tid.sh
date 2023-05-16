#!/bin/bash

dt=$(date '+%d/%m/%Y %H:%M:%S');

filebench_pid=$(filebench -f /filebench/workloads/fileserver.f > /dio_data/filebench_output.txt 2>&1 & echo $!);

echo "$dt | Filebench PID is $filebench_pid"

sleep 5s

declare -a PID_LIST=()

findpids() {
        for pid in /proc/$1/task/* ; do
                pid="$(basename "$pid")"
                PID_LIST+=($pid)
                for cpid in $(cat /proc/$1/task/$pid/children) ; do
                        findpids $cpid
                done
        done
}

findpids $filebench_pid

PID_LIST_LEN=${#PID_LIST[@]}
RANDOM_INDEX=$((1 + $RANDOM % $PID_LIST_LEN))

echo "$dt | PID list: ${PID_LIST[*]}"

echo "$dt | Running DIO for tid ${PID_LIST[$RANDOM_INDEX]}"
# /usr/share/dio/bin/dio --tid ${PID_LIST[$RANDOM_INDEX]}
export DIO_OPTIONS="--tid "${PID_LIST[$RANDOM_INDEX]}
/usr/share/dio/start_dio.sh

#!/bin/bash

set -e
set -m

DB_BENCH_COMMAND="/rocksdb/workloads/exec_workload.sh"

function check_workload() {
    if [ $1 == "load" ]; then
        DB_BENCH_COMMAND="/rocksdb/workloads/exec_workload.sh fillrandom 8 8 1024 fillrandom-8-8-1024"
    elif [ $1 == "ycsbwa" ]; then
        DB_BENCH_COMMAND="/rocksdb/workloads/exec_workload.sh ycsbwklda 8 8 1024 ycsbwklda-8-8-1024"
    else
        echo "Invalid workload"
        exit 1
    fi
}


if [ $# -eq 0 ]; then
    echo "No arguments supplied"
    exit 1
fi

if [ $# -eq 1 ]; then
    check_workload $1
    echo "Starting db_bench (vanilla mode)"
    $DB_BENCH_COMMAND
else
    if [ $1 == "dio" ]; then
        check_workload $2
        /usr/share/dio/start_dio.sh $DB_BENCH_COMMAND
    elif [ $1 == "strace" ]; then
        check_workload $2
        echo "Starting db_bench (strace mode)"
        mkdir -p /strace_data
        /rocksdb/truncate_file.sh strace.out > /strace_data/trunc.log 2>&1 &
        strace -yy -f -tt -s 0 -e trace=open,openat,creat,read,pread64,write,pwrite64,close -o strace.out -- $DB_BENCH_COMMAND

    else
        echo "Unknown command: $1"
        echo "\tRun without arguments to start Elasticsearch (vanilla mode)"
        echo "\tRun with 'dio' to start Elasticsearch with DIO"
        echo "\tRun with 'strace' to start Elasticsearch with strace"
        exit 1
    fi
fi



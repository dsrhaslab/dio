#!/bin/bash

set -e
set -m

FILEBENCH_COMMAND="filebench -f /filebench/workloads/fileserver.f"

if [ $# -eq 0 ]; then

    echo "Starting Filebench (vanilla mode)"
    $FILEBENCH_COMMAND

else

    if [ $1 == "dio" ]; then
        if [ $# -eq 1 ]; then
            echo "Starting Filebench (DIO mode)"
            exec /usr/share/dio/start_dio.sh $FILEBENCH_COMMAND

        elif [ $2 == "filter_tid" ]; then
            echo "Starting Filebench (DIO mode - Filter by TID)"
            exec /filebench/trace_filebench_tid.sh "DIO"
        else
            echo "Unknown option: $2"
        fi

    elif [ $1 == "strace" ]; then
        mkdir -p /strace_data
        if [ $# -eq 1 ]; then
            echo "Starting Filebench (strace mode)"
            echo "Strace options: $STRACE_OPTIONS"
            exec strace -f -tt $STRACE_OPTIONS -o /strace_data/strace.out -- $FILEBENCH_COMMAND
        elif [ $2 == "filter_tid" ]; then
            echo "Starting Filebench (DIO mode - Filter by TID)"
            exec /filebench/trace_filebench_tid.sh "strace"
        else
            echo "Unknown option: $2"
        fi
    else
        echo "Unknown command: $1"
        echo "\tRun without arguments to start Filebench (vanilla mode)"
        echo "\tRun with 'dio' to start Filebench with DIO"
        echo "\tRun with 'strace' to start Filebench with strace"
        exit 1

    fi

fi
#!/bin/bash

RES_DIR="/home"
ES_INDEX=$(date +"sysdig-%Y.%m.%d")

echo "$(date) | Getting sysdig stats from ES..."
echo "$(date) | ES_HOSTS: ${ES_HOSTS}"
echo "$(date) | ES_USERNAME: ${ES_USERNAME}"
echo "$(date) | ES_PASSWORD: ${ES_PASSWORD}"
echo "$(date) | ES_INDEX: ${ES_INDEX}"
echo "$(date) | Result file: ${RES_DIR}/sysdig_trace_stats.txt"

# This script is used to get the events stats from sysdig stored in elasticsearch

function count_events {
    response=$(curl -u "$ES_USERNAME:$ES_PASSWORD" -s -X GET "$ES_HOSTS/$ES_INDEX/_count")
    error=$(echo $response | jq '.error')
    error_type=$(echo $response | jq '.error.type')
    error_reason=$(echo $response | jq '.error.reason')
    op_count=0
    if [[ $error == "null" ]]; then
        op_count=$(echo $response | jq '.count')
    else
        echo "Erro: $error_type: $error_reason"
    fi
}

# #1 - syscall
function count_op {
    response=$(curl -u "$ES_USERNAME:$ES_PASSWORD" -s -X GET "$ES_HOSTS/$ES_INDEX/_count"  -H 'Content-Type: application/json' -d'
	{
        "query": {
            "bool": {
                "must": [
                    { "match": { "syscall.keyword": "'$1'" } }
                ]
            }
        }
    }
    ')
    error=$(echo $response | jq '.error')
    error_type=$(echo $response | jq '.error.type')
    error_reason=$(echo $response | jq '.error.reason')
    op_count=0
    if [[ $error == "null" ]]; then
        op_count=$(echo $response | jq '.count')
    else
        echo "Erro: $error_type: $error_reason"
    fi
}


function count_events_wo_procname {
    response=$(curl -u "$ES_USERNAME:$ES_PASSWORD" -s -X GET "$ES_HOSTS/$ES_INDEX/_count"  -H 'Content-Type: application/json' -d'
	{
        "query": {
            "bool": {
                "must_not": [
                    { "exists": { "field":  "procname" } }
                ]
            }
        }
    }
    ')
    error=$(echo $response | jq '.error')
    error_type=$(echo $response | jq '.error.type')
    error_reason=$(echo $response | jq '.error.reason')
    op_count=0
    if [[ $error == "null" ]]; then
        op_count=$(echo $response | jq '.count')
    else
        echo "Erro: $error_type: $error_reason"
    fi
}

function count_events_wo_retval {
    response=$(curl -u "$ES_USERNAME:$ES_PASSWORD" -s -X GET "$ES_HOSTS/$ES_INDEX/_count"  -H 'Content-Type: application/json' -d'
	{
        "query": {
            "bool": {
                "must": [
                    { "match": { "direction.keyword": "<" } }
                ],
                "must_not": [
                    { "exists": { "field":  "retval" } }
                ]
            }
        }
    }
    ')
    error=$(echo $response | jq '.error')
    error_type=$(echo $response | jq '.error.type')
    error_reason=$(echo $response | jq '.error.reason')
    op_count=0
    if [[ $error == "null" ]]; then
        op_count=$(echo $response | jq '.count')
    else
        echo "Erro: $error_type: $error_reason"
    fi
}

# $1 - syscall
# $2 - direction
function count_op_dir {
    response=$(curl -u "$ES_USERNAME:$ES_PASSWORD" -s -X GET "$ES_HOSTS/$ES_INDEX/_count"  -H 'Content-Type: application/json' -d'
	{
        "query": {
            "bool": {
                "must": [
                    { "match": { "syscall.keyword": "'$1'" } },
                    { "match": { "direction.keyword": "'$2'" } }
                ]
            }
        }
    }
    ')
    error=$(echo $response | jq '.error')
    error_type=$(echo $response | jq '.error.type')
    error_reason=$(echo $response | jq '.error.reason')
    op_dir=0
    if [[ $error == "null" ]]; then
        op_dir=$(echo $response | jq '.count')
    else
        echo "Erro: $error_type: $error_reason"
    fi
}

# $1 - syscall
# $2 - direction
# $3 - args
function count_op_dir_args {
    query='{
        "query": {
            "bool": {
                "must": [
                    { "match": { "syscall.keyword": "'$1'" } },
                    { "match": { "direction.keyword": "'$2'" } }
                ]
            }
        }
    }'

    arr=( "${@:3}" )

    for item in "${arr[@]}"
    do
        item_str='{ "exists": { "field": "'$item'" } }'
        query=$(echo "$query" | jq -r ".query.bool.must += [$item_str]")
    done

    response=$(curl -u "$ES_USERNAME:$ES_PASSWORD" -s -X GET "$ES_HOSTS/$ES_INDEX/_count"  -H 'Content-Type: application/json' -d"$query")
    error=$(echo $response | jq '.error')
    error_type=$(echo $response | jq '.error.type')
    error_reason=$(echo $response | jq '.error.reason')
    op_dir_args=0
    if [[ $error == "null" ]]; then
        op_dir_args=$(echo $response | jq '.count')
    else
        echo "Erro: $error_type: $error_reason"
    fi
}

function stats_all {
    echo -e "\n\n" >> $RES_DIR/sysdig_trace_stats.txt

    count_events
    total_events=$op_count
    echo "Total events: $total_events" >> $RES_DIR/sysdig_trace_stats.txt

    count_events_wo_procname
    total_events_wo_procname=$op_count
    echo "Total events without proc.name: $total_events_wo_procname" >> $RES_DIR/sysdig_trace_stats.txt

    count_events_wo_retval
    total_events_wo_retval=$op_count
    echo "Total events without retval: $total_events_wo_retval" >> $RES_DIR/sysdig_trace_stats.txt
}

function stats_op {
    op="$1"
    args_enter="$2"
    args_exit="$3"

    # Get the number of events
    count_op "$op"
    op_total=$op_count

    # Get the number of enter events
    count_op_dir "$op" ">"
    op_enter=$op_dir

    # Get the number of exit events
    count_op_dir "$op" "<"
    op_exit=$op_dir

    # Get the number of enter events with args
    op_enter_args=""
    if [ "$args_enter" != "" ]; then
        count_op_dir_args "$op" ">" $args_enter
        op_enter_args=$op_dir_args
    fi

    # Get the number of exit events with args
    op_exit_args=""
    if [ "$args_exit" != "" ]; then
        count_op_dir_args "$op" "<" $args_exit
        op_exit_args=$op_dir_args
    fi

    echo "$op $op_total $op_enter $op_enter_args $op_exit $op_exit_args" >> $RES_DIR/sysdig_trace_stats.txt
}

function all {
    echo "OP TOTAL ENTER ENTER_ARGS EXIT EXIT_ARGS" > $RES_DIR/sysdig_trace_stats.txt

    stats_op \
        "openat" \
        "flags mode" \
        "fd fdname flags mode retval"

    stats_op \
        "close" \
        "fd fdname" \
        "retval"

    stats_op \
        "write" \
        "fd fdname size" \
        "retval data"

    stats_op \
        "read" \
        "fd fdname size" \
        "retval data"

    stats_op \
        "stat" \
        "" \
        "retval path"

    stats_op \
        "lseek" \
        "fd fdname offset whence" \
        "retval"

    stats_op \
        "unlink" \
        "" \
        "retval path"

    stats_op \
        "pread" \
        "fd fdname size pos" \
        "retval data"

    stats_op \
        "fstat" \
        "fd fdname" \
        "retval"

    stats_op \
        "fsync" \
        "" \
        ""

    stats_op \
        "newfstatat" \
        "" \
        ""

    stats_op \
        "unlinkat" \
        "" \
        "retval name"

    stats_op \
        "lstat" \
        "" \
        "retval path"

    stats_all
}

"$@"
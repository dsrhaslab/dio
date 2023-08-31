#!/bin/bash

RES_DIR="/home"
FORMAT="*%evt.num %evt.time %evt.cpu %proc.name (%thread.tid) %evt.dir %evt.type %evt.rawres %evt.args"
FORMAT2="%evt.type"

echo "$(date) | Getting sysdig stats from file..."
echo "$(date) | Result file: ${RES_DIR}/sysdig_trace_stats.txt"


# This script is used to get the events stats from sysdig

function stats_all {
    echo -e "\n\n" >> $RES_DIR/sysdig_trace_stats.txt
    total_events=$( sysdig -r /home/sysdig_trace.scap -p "$FORMAT" | wc -l )
    echo "Total events: $total_events" >> $RES_DIR/sysdig_trace_stats.txt
    total_events_proc_name=$( sysdig -r /home/sysdig_trace.scap -p "$FORMAT" proc.name!="<NA>" | wc -l )
    echo "Total events with proc.name: $total_events_proc_name" >> $RES_DIR/sysdig_trace_stats.txt
    total_events_wo_res=$( sysdig -r /home/sysdig_trace.scap -p "$FORMAT2" evt.dir="<" and not evt.rawres exists | sort | uniq -c)
    echo -e "Total return events without res:\n$total_events_wo_res" >> $RES_DIR/sysdig_trace_stats.txt
    total_events_wo_fd_name=$( sysdig -r /home/sysdig_trace.scap -p "$FORMAT2" fd.num exists and not fd.name exists | sort | uniq -c)
    echo -e "Total events without fd.name:\n$total_events_wo_fd_name" >> $RES_DIR/sysdig_trace_stats.txt
}

function stats_op {
    op="$1"
    args_enter="$2"
    args_exit="$3"

    echo "$(date) | Getting stats for $op..."

    # Get the number of events
    op_total=$( sysdig -r /home/sysdig_trace.scap -p "$FORMAT" evt.type="$op" | wc -l )

    # Get the number of enter events
    op_enter=$( sysdig -r /home/sysdig_trace.scap -p "$FORMAT" evt.type="$op" and evt.dir=">" | wc -l )

    # Get the number of exit events
    op_exit=$( sysdig -r /home/sysdig_trace.scap -p "$FORMAT" evt.type="$op" and evt.dir="<" | wc -l )

    op_enter_args=""
    if [ "$args_enter" != "" ]; then
        # Get the number of enter events with args
        op_enter_args=$( sysdig -r /home/sysdig_trace.scap -p "$FORMAT" evt.type="$op" and evt.dir=">" and $args_enter | wc -l )
    fi

    op_exit_args=""
    if [ "$args_exit" != "" ]; then
        # Get the number of exit events with args
        op_exit_args=$( sysdig -r /home/sysdig_trace.scap -p "$FORMAT" evt.type="$op" and evt.dir="<" and $args_exit | wc -l )
    fi

    echo "$op $op_total $op_enter $op_enter_args $op_exit $op_exit_args" >> $RES_DIR/sysdig_trace_stats.txt
}

function all {
    echo "OP TOTAL ENTER ENTER_ARGS EXIT EXIT_ARGS" > $RES_DIR/sysdig_trace_stats.txt

    stats_op \
        "openat" \
        "evt.arg.flags exists and evt.arg.mode exists" \
        "fd.num exists and fd.name exists and evt.arg.flags exists and evt.arg.mode exists and evt.rawres exists"

    stats_op \
        "close" \
        "fd.num exists and fd.name exists" \
        "evt.rawres exists"

    stats_op \
        "write" \
        "fd.num exists and fd.name exists and evt.arg.size exists" \
        "evt.rawres exists and evt.arg.data exists"

    stats_op \
        "read" \
        "fd.num exists and fd.name exists and evt.arg.size exists" \
        "evt.rawres exists and evt.arg.data exists"

    # stats_op \
    #     "stat" \
    #     "" \
    #     "evt.rawres exists and evt.arg.path exists"

    stats_op \
        "lseek" \
        "fd.num exists and fd.name exists and evt.arg.offset exists and evt.arg.whence exists" \
        "evt.rawres exists"

    # stats_op \
    #     "unlink" \
    #     "" \
    #     "evt.rawres exists and evt.arg.path exists"

    stats_op \
        "pread" \
        "fd.num exists and fd.name exists and evt.arg.size exists and evt.arg.pos exists" \
        "evt.rawres exists and evt.arg.data exists"

    stats_op \
        "fstat" \
        "fd.num exists and fd.name exists" \
        "evt.rawres exists"

    # stats_op \
    #     "fsync" \
    #     "" \
    #     ""

    # stats_op \
    #     "newfstatat" \
    #     "" \
    #     ""

    # stats_op \
    #     "unlinkat" \
    #     "" \
    #     "evt.rawres exists and evt.arg.name exists"

    # stats_op \
    #     "lstat" \
    #     "" \
    #     "evt.rawres exists and evt.arg.path exists"

    stats_all
}

"$@"
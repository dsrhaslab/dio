#!/bin/bash

set -e
set -m

RUN=0
ES_URL="cloud124:31111"
ES_USERNAME="dio"
ES_PASSWORD="diopw"
# DIO_CONF="/home/$USER/ES/dio-es.yaml"
CUR_DIR=$(pwd)
TMP_DIR="$CUR_DIR/tmp"
RESULTS_DIR="$CUR_DIR/results"

mkdir -p $TMP_DIR
mkdir -p $RESULTS_DIR

VANILLA_CONTAINER="docker run -it -d --name es830 --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ taniaesteves/elasticsearch_dio:v1.0.0"

STRACE_CONTAINER="docker run -it -d --name es830 --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v $TMP_DIR/strace_data:/strace_data taniaesteves/elasticsearch_dio:v1.0.0 strace"

DIO_CONTAINER="docker run -it -d --name es830 --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v $TMP_DIR/dio_data:/dio_data -e CORRELATE_PATHS=true -e ES_URL=$ES_URL  taniaesteves/elasticsearch_dio:v1.0.0 dio --target_paths /usr/share/elasticsearch-8.3.0-SNAPSHOT"

SYSDIG_CONTAINER='docker run -it -d --name sysdig --privileged -v /var/run/docker.sock:/host/var/run/docker.sock -v /dev:/host/dev -v /proc:/host/proc:ro -v /boot:/host/boot:ro -v /lib/modules:/host/lib/modules:ro -v /usr:/host/usr:ro -v '$TMP_DIR'/sysdig_data:/home --net=host -e SYSDIG_BPF_PROBE="" sysdig/sysdig:0.31.4 sysdig -B -t a -p "*%evt.num %evt.outputtime %evt.cpu %proc.name (%thread.tid) %evt.dir %evt.type %evt.rawres %evt.args" container.name=es830 and "evt.type in ('"'open','openat','creat','read','pread','readv','write','pwrite','writev','lseek','truncate','ftruncate','rename','renameat','renameat2','close','unlink','unlinkat','stat','fstat','lstat','fstatfs','newfstatat','setxattr','getxattr','listxattr','removexattr','lsetxattr','lgetxattr','llistxattr','lremovexattr','fsetxattr','fgetxattr','flistxattr','fsync','fdatasync','readlink','readlinkat','mknod','mknodat'"')" -s 1 -w /home/sysdig_trace.scap'

BENCHMARK_CONTAINER="docker run --rm  --net=host elastic/rally race --track=geonames  --pipeline=benchmark-only --target-hosts=localhost:9200"

CURRENT_CONTAINER="$VANILLA_CONTAINER"

function test {
    echo "---- Starting elasticsearch container ($1-$RUN)"
    eval $CURRENT_CONTAINER
    sleep 120
    echo "---- Starting benchmark container ($1-$RUN)"
    $BENCHMARK_CONTAINER > $RESULTS_DIR/$1_bench_results_$RUN.txt 2>&1
    echo "---- Stopping elasticsearch container ($1-$RUN)"
    docker stop es830
    docker container wait es830
    docker rm es830
}

function vanilla {
    echo ">> Running test for vanilla ($RUN)"
    CURRENT_CONTAINER="$VANILLA_CONTAINER"
    test "vanilla"
    echo "<< Done"
}

function strace {
    echo ">> Running test for strace ($RUN)"
    CURRENT_CONTAINER="$STRACE_CONTAINER"
    test "strace"
    cp -r $TMP_DIR/strace_data $RESULTS_DIR/strace_data_$RUN
    echo "<< Done"
}

function dio {
    echo ">> Running test for DIO ($RUN)"
    CURRENT_CONTAINER="$DIO_CONTAINER"
    test "dio"
    cp -r $TMP_DIR/dio_data $RESULTS_DIR/dio_data_$RUN
    echo "<< Done"
}

function sysdig {
    echo ">> Running test for Sysdig (sysdig-$RUN)"
    CURRENT_CONTAINER="$VANILLA_CONTAINER"

    eval $SYSDIG_CONTAINER

    echo "---- Starting sysdig container (sysdig-$RUN)"
    sh -c 'echo $SYSDIG_CONTAINER'
    sleep 120
    test "sysdig"

    echo "---- Stopping sysdig container (sysdig-$RUN)"
    docker stop sysdig
    docker container wait sysdig
    docker rm sysdig
    cp -r $TMP_DIR/sysdig_data $RESULTS_DIR/sysdig_data_$RUN
    echo "<< Done"
}

function parse_sysdig {
    RUN=$1
    SYSDIG_PARSER_CONTAINER='docker run -it --rm --name sysdig --privileged -v /var/run/docker.sock:/host/var/run/docker.sock -v /dev:/host/dev -v /proc:/host/proc:ro -v /boot:/host/boot:ro -v /lib/modules:/host/lib/modules:ro -v /usr:/host/usr:ro -v '$RESULTS_DIR'/sysdig_data_'${RUN}':/home -v '$CUR_DIR'/parse_sysdig_trace.sh:/usr/share/sysdig/parse_sysdig_trace.sh --net=host -e SYSDIG_BPF_PROBE="" sysdig/sysdig:0.31.4 bash /usr/share/sysdig/parse_sysdig_trace.sh all'

    echo "---- Starting sysdig parser container (sysdig-$RUN)"
    echo "$SYSDIG_PARSER_CONTAINER"
    eval $SYSDIG_PARSER_CONTAINER
}

function parse_sysdig_all {
    parse_sysdig 1
    parse_sysdig 2
    parse_sysdig 3
}

function parse_strace {
    RUN=$1
    bash parse_strace_trace.sh -i "$RESULTS_DIR/strace_data_$RUN/strace.out" -o "$RESULTS_DIR/strace_data_$RUN/strace_trace_stats.txt"
}

function parse_strace_all {
    parse_strace 1
    parse_strace 2
    parse_strace 3
}


function delete_dio_indexes {
    curl -u "$ES_USERNAME:$ES_PASSWORD" -k -X DELETE "$ES_URL/dio*?pretty"
    sudo sh -c "echo 3 >'/proc/sys/vm/drop_caches' && swapoff -a && swapon -a && printf '\n%s\n' 'Ram-cache and Swap Cleared'"
}

# usage: run_test <test> <runs>
# example: run_test dio 3
function run_test {
    TEST=$1
    RUNS=$2
    sudo rm -rf $TMP_DIR/*
    mkdir -p $TMP_DIR/
    for i in $(seq 1 $RUNS);
    do
        if [ $TEST == "dio" ]
        then
            delete_dio_indexes
        fi
        RUN=$i
        echo "!! Starting run $RUN"
        $TEST
    done
}

function run_all {
    RUNS=$1
    run_test vanilla $RUNS
    run_test sysdig $RUNS
    run_test strace $RUNS
    run_test dio $RUNS
}

$@

#!/bin/bash

set -e
set -m

RUN=0
ES_URL="cloud124:31111"
ES_USERNAME="dio"
ES_PASSWORD="diopw"
CUR_DIR=$(pwd)
TMP_DIR="$CUR_DIR/tmp"
RESULTS_DIR="$CUR_DIR/results"
SLEEP_TIME=120

mkdir -p $TMP_DIR
mkdir -p $RESULTS_DIR

VANILLA_CONTAINER="docker run -it -d --name redis-server  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ taniaesteves/redis_dio:v1"

STRACE_CONTAINER="docker run -it -d --name redis-server  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v $TMP_DIR/strace_data:/strace_data taniaesteves/redis_dio:v1 strace"

DIO_CONTAINER="docker run -it -d --name redis-server  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v $TMP_DIR/dio_data:/dio_data -e CORRELATE_PATHS=true -e ES_URL=$ES_URL  taniaesteves/redis_dio:v1 dio"

DIO_CONTAINER_V2="docker run -it -d --name redis-server  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v $TMP_DIR/dio_data:/dio_data -e CORRELATE_PATHS=true -e ES_URL=$ES_URL  taniaesteves/redis_dio:v2 dio"

SYSDIG_CONTAINER='docker run -it -d --name sysdig --privileged -v /var/run/docker.sock:/host/var/run/docker.sock -v /dev:/host/dev -v /proc:/host/proc:ro -v /boot:/host/boot:ro -v /lib/modules:/host/lib/modules:ro -v /usr:/host/usr:ro -v '$TMP_DIR'/sysdig_data:/home --net=host -e SYSDIG_BPF_PROBE="" sysdig/sysdig:0.31.4 sysdig -B -t a -p "*%evt.num %evt.outputtime %evt.cpu %proc.name (%thread.tid) %evt.dir %evt.type %evt.rawres %evt.args" container.name="redis-server" and "evt.type in ('"'open','openat','creat','read','pread','readv','write','pwrite','writev','lseek','truncate','ftruncate','rename','renameat','renameat2','close','unlink','unlinkat','stat','fstat','lstat','fstatfs','newfstatat','setxattr','getxattr','listxattr','removexattr','lsetxattr','lgetxattr','llistxattr','lremovexattr','fsetxattr','fgetxattr','flistxattr','fsync','fdatasync','readlink','readlinkat','mknod','mknodat'"')" and "fd.type in  ('"'file','directory'"')" -s 1 -w /home/sysdig_trace.scap'

BENCHMARK_CONTAINER="docker run -it --rm --name redis-bench --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ taniaesteves/redis_dio:v1 benchmark"

CURRENT_CONTAINER="$VANILLA_CONTAINER"

function test {
    echo "---- Starting redis container ($1-$RUN)"
    $CURRENT_CONTAINER
    sleep $SLEEP_TIME
    echo "---- Starting benchmark container ($1-$RUN)"
    $BENCHMARK_CONTAINER > $RESULTS_DIR/$1_bench_results_$RUN.txt 2>&1
    echo "---- Stopping redis container ($1-$RUN)"
    docker stop redis-server
    docker container wait redis-server
    docker rm redis-server
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


    echo "---- Starting sysdig container (sysdig-$RUN)"
    echo "$SYSDIG_CONTAINER"
    eval $SYSDIG_CONTAINER
    sleep $SLEEP_TIME
    test "sysdig"

    echo "---- Stopping sysdig container (sysdig-$RUN)"
    docker stop sysdig
    docker container wait sysdig
    cp -r $TMP_DIR/sysdig_data $RESULTS_DIR/sysdig_data_$RUN
    docker logs sysdig > $RESULTS_DIR/sysdig_data_$RUN/sysdig_logs.txt
    docker rm sysdig
    echo "<< Done"
}

function parse_sysdig {
    RUN=$1
    SYSDIG_PARSER_CONTAINER='docker run -it --rm --name sysdig --privileged -v /var/run/docker.sock:/host/var/run/docker.sock -v /dev:/host/dev -v /proc:/host/proc:ro -v /boot:/host/boot:ro -v /lib/modules:/host/lib/modules:ro -v /usr:/host/usr:ro -v '$RESULTS_DIR'/sysdig_data_'${RUN}':/home -v '$CUR_DIR'/parse_sysdig_trace.sh:/usr/share/sysdig/parse_sysdig.sh --net=host -e SYSDIG_BPF_PROBE="" sysdig/sysdig:0.31.4 bash /usr/share/sysdig/parse_sysdig.sh all'

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

function version1 {
    echo ">> Running test for DIO Redis-v1 ($RUN)"
    CURRENT_CONTAINER="$DIO_CONTAINER"
    test "dio"
    cp -r $TMP_DIR/dio_data $RESULTS_DIR/dio_data_v1_$RUN
    echo "<< Done"
}

function version2 {
    echo ">> Running test for DIO Redis-v2 ($RUN)"
    CURRENT_CONTAINER="$DIO_CONTAINER_V2"
    test "dio"
    cp -r $TMP_DIR/dio_data $RESULTS_DIR/dio_data_v2_$RUN
    echo "<< Done"
}

function use_case {
    delete_dio_indexes
    version1
    version2
}


# usage: run_test <test> <runs>
# example: run_test dio 3
function run_test {
    echo "Running test $1 for $2 runs"
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
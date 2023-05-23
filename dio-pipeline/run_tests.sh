#!/bin/bash

LOGS_DIR="final_test_results/ansible_logs"
HOSTS="hosts.ini"
mkdir -p $LOGS_DIR

STARTING_RUN=1
RUNS=3

# --------

function reset_kube_cluster {
    # destroy previous dio pipeline
    ansible-playbook -u gsd -i $HOSTS dio_playbook.yml --tags delete_dio -e run_all=true

    ansible-playbook -u gsd -i $HOSTS reset-site.yaml

    if [ $? -eq 0 ]; then
        echo OK
    else
        ansible-playbook -u gsd -i $HOSTS reset-site.yaml
        if [ $? -eq 0 ]; then
            echo OK
        else
            echo "FAILED to create the cluster"
            exit 1
        fi
    fi


}

function setup_kube_cluster {
    # reset kubernetes cluster
    reset_kube_cluster

    # create kubernetes cluster
    ansible-playbook -u gsd -i $HOSTS playbook.yml

    # prepare setup
    ansible-playbook -u gsd -i $HOSTS dio_playbook.yml --tags prepare_setup

    # create new dio pipeline
    ansible-playbook -u gsd -i $HOSTS dio_playbook.yml --tags deploy_dio -e run_all=true
}

function mount_dio_pipeline {

    # destroy previous dio pipeline
    ansible-playbook -u gsd -i $HOSTS dio_playbook.yml --tags delete_dio -e run_all=true

    # create new dio pipeline
    ansible-playbook -u gsd -i $HOSTS dio_playbook.yml --tags deploy_dio -e run_all=true
}

# --------

function rocksdb () {
    reset_kube_cluster
    ansible-playbook -u gsd -i $HOSTS rocksdb_dio_playbook.yml --tags load | tee "$LOGS_DIR/rocksdb_load_"$i".txt" ;

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        reset_kube_cluster

        ansible-playbook -u gsd -i $HOSTS rocksdb_dio_playbook.yml --tags vanilla -e run_number="$i" -e prefix="$TEST_NR" | tee "$LOGS_DIR/rocksdb_vanilla_"$i".txt" ;

        ansible-playbook -u gsd -i $HOSTS rocksdb_dio_playbook.yml --tags sysdig -e run_number="$i" -e prefix="$TEST_NR" | tee "$LOGS_DIR/rocksdb_sysdig_"$i".txt" ;

        ansible-playbook -u gsd -i $HOSTS rocksdb_dio_playbook.yml --tags strace -e run_number="$i" -e prefix="$TEST_NR" | tee "$LOGS_DIR/rocksdb_strace_"$i".txt" ;

        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS rocksdb_dio_playbook.yml --tags dio -e run_number="$i" -e prefix="$TEST_NR" | tee "$LOGS_DIR/rocksdb_dio_"$i".txt" ;
    done

}

# --------MICROS

function help () {

    echo "Script for running DIO experiments."
    echo
    echo "Usage: ./dio_experiments.sh [OPTION]"
    echo
    echo "Options:"
    echo "  - help, prints this help message"
    echo "  - dio_setups_experiments, runs dio detailed setups experiments (including vanilla, dio_elk and dio_file)"
    echo "  - dio_filters, runs dio filters experiments (including dio_elk_filters and dio_file_filters)"
    echo "  - dio_rate_limit, runs dio rate limit experiments (for rates 12500, 15000, 17500, 20000, 22500, 25000, 50000, 100000)"
}

function micro_rw {
    mkdir -p $LOGS_DIR

    TEST_NR="t01-"
    echo "$(date) | $TEST_NR Starting DIO RW experiments" >> $LOGS_DIR/filebench-tests.log
    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do

        # ---- VANILLA
        echo "$(date) | Filebench - Vanilla - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags vanilla -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"vanilla-"$i".txt";

        # ---- SYSDIG
        echo "$(date) | Filebench - Sysdig (detailedPall) - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags sysdig_detailedPall -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-detailedPall-"$i".txt";

        echo "$(date) | Filebench - Sysdig (detailedPallCplain) - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags sysdig_detailedPallCplain -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-detailedPallCplain-"$i".txt";

        # ---- STRACE
        echo "$(date) | Filebench - Strace (raw) - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags strace_raw -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"strace-raw-"$i".txt";

        echo "$(date) | Filebench - Strace (detailedPargs) - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags strace_detailedPargs -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"strace-detailedPargs-"$i".txt";

        echo "$(date) | Filebench - Strace (detailedPall) - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags strace_detailedPall -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"strace-detailedPall-"$i".txt";

        echo "$(date) | Filebench - Strace (detailedPallCplain) - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags strace_detailedPallCplain -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"strace-detailedPallCplain-"$i".txt";

        # ---- DIO - ELK
        echo "$(date) | Filebench - DIO (raw) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_raw -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-raw-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPfds) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPfds -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPfds-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPall) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPallCuhash) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPallCuhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPallCuhash-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPallCkhash) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPallCkhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPallCkhash-"$i".txt";
    done
}

function micro_vanilla_dio_rt {
    mkdir -p $LOGS_DIR
    FILEBENCH_RATE_LIMITE=$1
    FILEBENCH_EVENT_GEN_RATE=$2

    sufix=""
    if [ "$FILEBENCH_RATE_LIMITE" == "true" ]; then
        sufix="_rate_limited_"$FILEBENCH_EVENT_GEN_RATE
    fi

    TEST_NR="t02-"
    echo "$(date) | $TEST_NR Starting DIO RT experiments" >> $LOGS_DIR/filebench-tests.log
    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do

        # ---- Vanilla
        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - Vanilla - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags vanilla -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"vanilla$sufix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (raw) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_raw -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-raw$sufix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (detailedPfds) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPfds -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPfds$sufix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (detailedPall) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall$sufix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (detailedPallCuhash) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPallCuhash -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPallCuhash$sufix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (raw) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPallCkhash -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPallCkhash$sufix-"$i".txt" ;

    done
}

function micro_rt {
    dio_rate_limit "true"  25000
    dio_rate_limit "true"  30000
    dio_rate_limit "true"  35000
    dio_rate_limit "true"  40000
    dio_rate_limit "true"  45000
    dio_rate_limit "true"  50000
    dio_rate_limit "true" 100000
}

function micro_dio_storage_backends {
    mkdir -p $LOGS_DIR
    reset_kube_cluster

    TEST_NR="t03-"
    echo "$(date) | $TEST_NR Starting DIO Storage backends experiments" >> $LOGS_DIR/filebench-tests.log
    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do

        # ---- raw

        echo "$(date) | Filebench - DIO - Raw - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_raw -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-raw-"$i".txt";

        echo "$(date) | Filebench - DIO - Raw - NOP - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_nop_raw -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-nop-raw-"$i".txt";

        # ---- detailedPfds

        echo "$(date) | Filebench - DIO - detailedPfds - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_detailedPfds -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-detailedPfds-"$i".txt";

        echo "$(date) | Filebench - DIO - detailedPfdsPfds - NOP - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_nop_detailedPfds -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-nop-detailedPfds-"$i".txt";

        # ---- detailedPall

        echo "$(date) | Filebench - DIO - detailedPall - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_detailedPall -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-detailedPall-"$i".txt";

        echo "$(date) | Filebench - DIO - detailedPall - NOP - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_nop_detailedPall -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-nop-detailedPall-"$i".txt";

        # ---- detailedPallCuhash

        echo "$(date) | Filebench - DIO - detailedPallCuhash - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_detailedPallCuhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-detailedPallCuhash-"$i".txt";

        echo "$(date) | Filebench - DIO - detailedPallCuhash - NOP - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_nop_detailedPallCuhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-nop-detailedPallCuhash-"$i".txt";

        # ---- detailedPallCkhash

        echo "$(date) | Filebench - DIO - detailedPallCkhash - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_detailedPallCkhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-detailedPallCkhash-"$i".txt";

        echo "$(date) | Filebench - DIO - detailedPallCkhash - NOP - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_nop_detailedPallCkhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-nop-detailedPallCkhash-"$i".txt";
    done
}

function micro_filters {
    mkdir -p $LOGS_DIR

    TEST_NR="t04-"
    echo "$(date) | $TEST_NR Starting Filters experiments (strace, sysdig, DIO)" >> $LOGS_DIR/filebench-tests.log
    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do

        # ---- TID FILTER
        # Strace
        echo "$(date) | Filebench - Strace (detailedPall) - tid filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags strace_detailedPall_tid_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"strace-detailedPall-tid-filter-"$i".txt" ;

        # Sysdig
        echo "$(date) | Filebench - Sysdig (detailedPall) - tid filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags sysdig_detailedPall_tid_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-detailedPall-tid-filter-"$i".txt" ;

        # DIO
        echo "$(date) | Filebench - DIO (detailedPall) - tid filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_tid_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-tid-filter-"$i".txt" ;


        # ---- ORWC FILTER
        # Strace
        echo "$(date) | Filebench - Strace (detailedPall) - orwc filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags strace_detailedPall_orwc_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"strace-detailedPall-orwc-filter-"$i".txt" ;

        # Sysdig
        echo "$(date) | Filebench - Sysdig (detailedPall) - orwc filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags sysdig_detailedPall_orwc_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-detailedPall-orwc-filter-"$i".txt" ;

        # DIO
        echo "$(date) | Filebench - DIO (detailedPall) - orwc filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_orwc_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-orwc-filter-"$i".txt" ;


        # ---- READ FILTER
        # Strace
        echo "$(date) | Filebench - Strace (detailedPall) - read filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags strace_detailedPall_read_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"strace-detailedPall-read-filter-"$i".txt" ;

        # Sysdig
        echo "$(date) | Filebench - Sysdig (detailedPall) - read filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags sysdig_detailedPall_read_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-detailedPall-read-filter-"$i".txt" ;

        # DIO
        echo "$(date) | Filebench - DIO (detailedPall) - read filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_read_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-read-filter-"$i".txt" ;


        # ---- PASSIVE FILTER
        # Strace
        echo "$(date) | Filebench - Strace (detailedPall) - passive filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags strace_detailedPall_passive_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"strace-detailedPall-passive-filter-"$i".txt" ;

        # Sysdig
        echo "$(date) | Filebench - Sysdig (detailedPall) - passive filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags sysdig_detailedPall_passive_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-detailedPall-passive-filter-"$i".txt" ;

        # DIO
        echo "$(date) | Filebench - DIO (detailedPall) - passive filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_passive_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-passive-filter-"$i".txt" ;
    done
}

function micro_dio_ringbuf {
    mkdir -p $LOGS_DIR

    TEST_NR="t05-"
    echo "$(date) | $TEST_NR Starting RB experiments" >> $LOGS_DIR/filebench-tests.log
    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        # ---- RB 1KB
        echo "$(date) | Filebench - DIO - RB 1KB - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_rb1kb -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-rb1kb-"$i".txt" ;

        # ---- RB 16KB
        echo "$(date) | Filebench - DIO - RB 16KB - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_rb16kb -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-rb16kb-"$i".txt" ;

        # ---- RB 32KB
        echo "$(date) | Filebench - DIO - RB 32KB - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_rb32kb -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-rb32kb-"$i".txt" ;

        # ---- RB 64KB
        echo "$(date) | Filebench - DIO - RB 64KB - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_rb64kb -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-rb64kb-"$i".txt" ;

        # ---- RB 128KB
        echo "$(date) | Filebench - DIO - RB 128KB - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_rb128kb -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-rb128kb-"$i".txt" ;

        # ---- RB 256KB
        echo "$(date) | Filebench - DIO - RB 256KB - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_rb256kb -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-rb256kb-"$i".txt" ;

        # ---- RB 512KB
        echo "$(date) | Filebench - DIO - RB 512KB - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_rb512kb -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-rb512kb-"$i".txt" ;
    done
}


"$@"
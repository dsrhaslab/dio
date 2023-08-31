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

        # ---- SYSDIG - File

        echo "$(date) | Filebench - Sysdig (detailedPall - File) - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags sysdig_file_detailedPall -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-file-detailedPall-"$i".txt";

        echo "$(date) | Filebench - Sysdig (detailedPallCplain - File) - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags sysdig_file_detailedPallCplain -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-file-detailedPallCplain-"$i".txt";

        # ---- SYSDIG - ELK

        echo "$(date) | Filebench - Sysdig (detailedPall - ELK) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags sysdig_elk_detailedPall -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-elk-detailedPall-"$i".txt";

        echo "$(date) | Filebench - Sysdig (detailedPallCplain - ELK) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags sysdig_elk_detailedPallCplain -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"sysdig-elk-detailedPallCplain-"$i".txt";

        # ---- SYSDIG - /dev/null

        echo "$(date) | Filebench - Sysdig (detailedPall - FILE+/dev/null) - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags sysdig_file_dev_null_detailedPall -e run_number="$i" -e prefix="$TEST_NR" -e ls_batch_size=$LS_BATCH_SIZE -e ls_batch_delay=$LS_BATCH_DELAY | tee $LOGS_DIR"/"$TEST_NR"sysdig-file-dev-null-detailedPall-"$i".txt";

        # ---- DIO - File

        echo "$(date) | Filebench - DIO (raw) - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_raw -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-raw-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPfds) - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_detailedPfds -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-detailedPfds-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPall) - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_detailedPall -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-detailedPall-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPallCuhash) - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_detailedPallCuhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-detailedPallCuhash-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPallCkhash) - FILE - Run $i" >> $LOGS_DIR/filebench-tests.log
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_file_detailedPallCkhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-file-detailedPallCkhash-"$i".txt";

        # ---- DIO - ELK

        echo "$(date) | Filebench - DIO (raw) - ELK - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_raw -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-raw-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPfds) - ELK - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPfds -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPfds-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPall) - ELK - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPallCuhash) - ELK - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPallCuhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPallCuhash-"$i".txt";

        echo "$(date) | Filebench - DIO (detailedPallCkhash) - ELK - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPallCkhash -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPallCkhash-"$i".txt";
    done
}

function dio_elk_bs {
    ES_FLUSH_BYTES=$1
    ES_FLUSH_INTERVAL=$2
    RUN=$3
    TEST_NR=$4
    suffix="_fb"$ES_FLUSH_BYTES"_fi"$ES_FLUSH_INTERVAL

    echo "$(date) | Filebench - DIO (detailedPall - elk - FB=$ES_FLUSH_BYTES FI=$ES_FLUSH_INTERVAL) - Run $RUN" >> $LOGS_DIR/filebench-tests.log
    setup_kube_cluster
    ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall -e run_number="$RUN" -e prefix="$TEST_NR" -e suffix="$suffix" -e dio_es_flush_bytes=$ES_FLUSH_BYTES -e dio_es_flush_interval=$ES_FLUSH_INTERVAL | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall$suffix-"$i".txt" ;
}

function micro_dio_elk_bs {
    TEST_NR="t06-"
    echo "$(date) | $TEST_NR Starting DIO BS experiments" >> $LOGS_DIR/filebench-tests.log
    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        dio_elk_bs  1000000 30 "$i" "$TEST_NR"
        dio_elk_bs  4000000 30 "$i" "$TEST_NR"
        dio_elk_bs 10000000 30 "$i" "$TEST_NR"
        dio_elk_bs 15000000 30 "$i" "$TEST_NR"
    done
}

function sysdig_elk_bs {
    LS_BATCH_SIZE=$1
    LS_BATCH_DELAY=$2
    RUN=$3
    TEST_NR=$4
    suffix="_bs"$LS_BATCH_SIZE"_bd"$LS_BATCH_DELAY

    echo "$(date) | Filebench - Sysdig (detailedPall - elk - BS=$LS_BATCH_SIZE BD=$LS_BATCH_DELAY) - Run $RUN" >> $LOGS_DIR/filebench-tests.log
    setup_kube_cluster
    ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags sysdig_elk_detailedPall -e run_number="$RUN" -e prefix="$TEST_NR" -e ls_batch_size=$LS_BATCH_SIZE -e ls_batch_delay=$LS_BATCH_DELAY | tee $LOGS_DIR"/"$TEST_NR"sysdig-elk-detailedPall$suffix-"$RUN".txt";
}

function micro_sysdig_elk_bs {
    TEST_NR="t06-"
    echo "$(date) | $TEST_NR Starting Sysdig BS experiments" >> $LOGS_DIR/filebench-tests.log
    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        sysdig_elk_bs  125 50 "$i" "$TEST_NR"
        sysdig_elk_bs  250 50 "$i" "$TEST_NR"
        sysdig_elk_bs  500 50 "$i" "$TEST_NR"
        sysdig_elk_bs 1000 50 "$i" "$TEST_NR"
        sysdig_elk_bs 2000 50 "$i" "$TEST_NR"
        sysdig_elk_bs 4000 50 "$i" "$TEST_NR"
        sysdig_elk_bs 15000 50 "$i" "$TEST_NR"
    done
}

function vanilla_dio_rt {
    mkdir -p $LOGS_DIR
    FILEBENCH_RATE_LIMITE=$1
    FILEBENCH_EVENT_GEN_RATE=$2

    suffix=""
    if [ "$FILEBENCH_RATE_LIMITE" == "true" ]; then
        suffix="_rate_limited_"$FILEBENCH_EVENT_GEN_RATE
    fi

    TEST_NR="t02-"
    echo "$(date) | $TEST_NR Starting DIO RT experiments" >> $LOGS_DIR/filebench-tests.log
    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do

        # ---- Vanilla
        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - Vanilla - Run $i" >> $LOGS_DIR/filebench-tests.log
        reset_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml  --tags vanilla -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"vanilla$suffix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (raw) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_raw -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-raw$suffix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (detailedPfds) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPfds -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPfds$suffix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (detailedPall) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall$suffix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (detailedPallCuhash) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPallCuhash -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPallCuhash$suffix-"$i".txt" ;

        echo "$(date) | Filebench - RT $FILEBENCH_EVENT_GEN_RATE - DIO (raw) - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPallCkhash -e run_number="$i" -e prefix="$TEST_NR" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPallCkhash$suffix-"$i".txt" ;

    done
}

function micro_rt {
    vanilla_dio_rt "true"  25000
    vanilla_dio_rt "true"  30000
    vanilla_dio_rt "true"  35000
    vanilla_dio_rt "true"  40000
    vanilla_dio_rt "true"  45000
    vanilla_dio_rt "true"  50000
    vanilla_dio_rt "true" 100000
}

function micro_filters {
    mkdir -p $LOGS_DIR

    TEST_NR="t04-"
    echo "$(date) | $TEST_NR Starting Filters experiments (strace, sysdig, DIO)" >> $LOGS_DIR/filebench-tests.log
    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do

        # ---- TID FILTER

        echo "$(date) | Filebench - DIO (detailedPall) - tid filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_tid_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-tid-filter-"$i".txt" ;

        # ---- ORWC FILTER

        echo "$(date) | Filebench - DIO (detailedPall) - orwc filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_orwc_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-orwc-filter-"$i".txt" ;

        # ---- READ FILTER

        echo "$(date) | Filebench - DIO (detailedPall) - read filter - Run $i" >> $LOGS_DIR/filebench-tests.log
        setup_kube_cluster
        ansible-playbook -u gsd -i $HOSTS filebench_playbook.yml --tags dio_elk_detailedPall_read_filter -e run_number="$i" -e prefix="$TEST_NR" | tee $LOGS_DIR"/"$TEST_NR"dio-elk-detailedPall-read-filter-"$i".txt" ;

        # ---- PASSIVE FILTER

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
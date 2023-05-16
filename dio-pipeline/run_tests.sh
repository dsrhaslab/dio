#!/bin/bash

LOGS_DIR="final_test_results/ansible_logs"

mkdir -p $LOGS_DIR

STARTING_RUN=1
RUNS=3

# --------

function reset_kube_cluster {
    # destroy previous dio pipeline
    ansible-playbook -u gsd -i hosts.ini dio_playbook.yml --tags delete_dio -e run_all=true

    ansible-playbook -u gsd -i hosts.ini reset-site.yaml

    if [ $? -eq 0 ]; then
        echo OK
    else
        ansible-playbook -u gsd -i hosts.ini reset-site.yaml
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
    ansible-playbook -u gsd -i hosts.ini playbook.yml

    # prepare setup
    ansible-playbook -u gsd -i hosts.ini dio_playbook.yml --tags prepare_setup

    # create new dio pipeline
    ansible-playbook -u gsd -i hosts.ini dio_playbook.yml --tags deploy_dio -e run_all=true
}

function mount_dio_pipeline {

    # destroy previous dio pipeline
    ansible-playbook -u gsd -i hosts.ini dio_playbook.yml --tags delete_dio -e run_all=true

    # create new dio pipeline
    ansible-playbook -u gsd -i hosts.ini dio_playbook.yml --tags deploy_dio -e run_all=true
}

# --------

function rocksdb () {
    reset_kube_cluster
    ansible-playbook -u gsd -i hosts.ini rocksdb_dio_playbook.yml --tags load | tee "$LOGS_DIR/rocksdb_load_"$i".txt" ;

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        reset_kube_cluster

        ansible-playbook -u gsd -i hosts.ini rocksdb_dio_playbook.yml --tags vanilla -e run_number="$i" | tee "$LOGS_DIR/rocksdb_vanilla_"$i".txt" ;

        ansible-playbook -u gsd -i hosts.ini rocksdb_dio_playbook.yml --tags sysdig -e run_number="$i" | tee "$LOGS_DIR/rocksdb_sysdig_"$i".txt" ;

        ansible-playbook -u gsd -i hosts.ini rocksdb_dio_playbook.yml --tags strace -e run_number="$i" | tee "$LOGS_DIR/rocksdb_strace_"$i".txt" ;

        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini rocksdb_dio_playbook.yml --tags dio -e run_number="$i" | tee "$LOGS_DIR/rocksdb_dio_"$i".txt" ;
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


function vanilla {

    # reset kubernetes cluster
    reset_kube_cluster
    FILEBENCH_RATE_LIMITE=$1
    FILEBENCH_EVENT_GEN_RATE=$2

    sufix=""
    if [ "$FILEBENCH_RATE_LIMITE" == "true" ]; then
        sufix="_rate_limited_"$FILEBENCH_EVENT_GEN_RATE
    fi

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        echo "Filebench - Vanilla - Run $i"
        ansible-playbook -u gsd filebench_playbook.yml  --tags vanilla -e run_number="$i" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee "$LOGS_DIR/t00_vanilla_$i$sufix.txt" ;
    done
}

function strace {
    # reset kubernetes cluster
    reset_kube_cluster

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        echo "Filebench - Strace - Run $i"
        ansible-playbook -u gsd filebench_playbook.yml  --tags strace -e run_number="$i" | tee "$LOGS_DIR/strace_$i.txt" ;
    done
}

function dio_raw {
    mkdir -p $LOGS_DIR
    FILEBENCH_RATE_LIMITE=$1
    FILEBENCH_EVENT_GEN_RATE=$2

    sufix=""
    if [ "$FILEBENCH_RATE_LIMITE" == "true" ]; then
        sufix="-ratelim"$FILEBENCH_EVENT_GEN_RATE
    fi

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        reset_kube_cluster

        # NOP
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_raw -e run_number="$i" | tee "$LOGS_DIR/t03_dio-nop-raw$sufix-"$i".txt" ;

        # FILE
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_raw -e run_number="$i" | tee "$LOGS_DIR/t03_dio-file-raw$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_raw -e run_number="$i" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee "$LOGS_DIR/t03_dio-elk-raw$sufix-"$i".txt" ;
    done
}

function dio_detailed {
    mkdir -p $LOGS_DIR
    FILEBENCH_RATE_LIMITE=$1
    FILEBENCH_EVENT_GEN_RATE=$2

    sufix=""
    if [ "$FILEBENCH_RATE_LIMITE" == "true" ]; then
        sufix="-ratelim"$FILEBENCH_EVENT_GEN_RATE
    fi

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        reset_kube_cluster

        # NOP
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed -e run_number="$i" | tee "$LOGS_DIR/t04_dio-nop-detailed$sufix-"$i".txt" ;

        # FILE
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed -e run_number="$i" | tee "$LOGS_DIR/t04_dio-file-detailed$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed -e run_number="$i" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee "$LOGS_DIR/t04_dio-elk-detailed$sufix-"$i".txt" ;
    done
}

function dio_detailed_paths {
    mkdir -p $LOGS_DIR
    FILEBENCH_RATE_LIMITE=$1
    FILEBENCH_EVENT_GEN_RATE=$2

    sufix=""
    if [ "$FILEBENCH_RATE_LIMITE" == "true" ]; then
        sufix="-ratelim"$FILEBENCH_EVENT_GEN_RATE
    fi

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        reset_kube_cluster

        # NOP
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed-paths -e run_number="$i" | tee "$LOGS_DIR/t05_dio-nop-detailed-paths$sufix-"$i".txt" ;

        # FILE
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed-paths -e run_number="$i" | tee "$LOGS_DIR/t05_dio-file-detailed-paths$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed-paths -e run_number="$i" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee "$LOGS_DIR/t05_dio-elk-detailed-paths$sufix-"$i".txt" ;
    done
}

function dio_detailed_paths_uhash {
    mkdir -p $LOGS_DIR
    FILEBENCH_RATE_LIMITE=$1
    FILEBENCH_EVENT_GEN_RATE=$2

    sufix=""
    if [ "$FILEBENCH_RATE_LIMITE" == "true" ]; then
        sufix="-ratelim"$FILEBENCH_EVENT_GEN_RATE
    fi

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        reset_kube_cluster

        # NOP
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed-paths-uhash -e run_number="$i" | tee "$LOGS_DIR/t06_dio-nop-detailed-paths-uhash$sufix-"$i".txt" ;

        # FILE
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed-paths-uhash -e run_number="$i" | tee "$LOGS_DIR/t06_dio-file-detailed-paths-uhash$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed-paths-uhash -e run_number="$i" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee "$LOGS_DIR/t06_dio-elk-detailed-paths-uhash$sufix-"$i".txt" ;
    done
}


function dio_detailed_paths_khash {
    mkdir -p $LOGS_DIR
    FILEBENCH_RATE_LIMITE=$1
    FILEBENCH_EVENT_GEN_RATE=$2

    sufix=""
    if [ "$FILEBENCH_RATE_LIMITE" == "true" ]; then
        sufix="-ratelim"$FILEBENCH_EVENT_GEN_RATE
    fi

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        reset_kube_cluster

        # NOP
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed-paths-khash -e run_number="$i" | tee "$LOGS_DIR/t07_dio-nop-detailed-paths-khash$sufix-"$i".txt" ;

        # FILE
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed-paths-khash -e run_number="$i" | tee "$LOGS_DIR/t07_dio-file-detailed-paths-khash$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed-paths-khash -e run_number="$i" -e filebench_rate_limit=$FILEBENCH_RATE_LIMITE -e filebench_event_rate=$FILEBENCH_EVENT_GEN_RATE | tee "$LOGS_DIR/t07_dio-elk-detailed-paths-khash$sufix-"$i".txt" ;
    done
}


function micro {
    mkdir -p $LOGS_DIR
    FILEBENCH_RATE_LIMITE=$1
    FILEBENCH_EVENT_GEN_RATE=$2

    sufix=""
    if [ "$FILEBENCH_RATE_LIMITE" == "true" ]; then
        sufix="-ratelim"$FILEBENCH_EVENT_GEN_RATE
    fi

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do

        # ---- T0 - VANILLA
        reset_kube_cluster
        ansible-playbook -u gsd filebench_playbook.yml  --tags vanilla -e run_number="$i" | tee "$LOGS_DIR/t00-vanilla-$i$sufix.txt" ;

        # # ---- T1 - STRACE
        # reset_kube_cluster
        # ansible-playbook -u gsd filebench_playbook.yml  --tags strace -e run_number="$i" | tee "$LOGS_DIR/t01-strace-$i$sufix.txt" ;

        # # ---- T2 - SYSDIG

        # ---- T3 - RAW
        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_raw -e run_number="$i" | tee "$LOGS_DIR/t03-dio-nop-raw$sufix-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_raw -e run_number="$i" | tee "$LOGS_DIR/t03-dio-file-raw$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_raw -e run_number="$i" | tee "$LOGS_DIR/t03-dio-elk-raw$sufix-"$i".txt" ;

        # ---- T4 - DETAILED
        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed -e run_number="$i" | tee "$LOGS_DIR/t04-dio-nop-detailed$sufix-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed -e run_number="$i" | tee "$LOGS_DIR/t04-dio-file-detailed$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed -e run_number="$i" | tee "$LOGS_DIR/t04-dio-elk-detailed$sufix-"$i".txt" ;


        # ---- T5 - DETAILED-PATHS
        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed_paths -e run_number="$i" | tee "$LOGS_DIR/t05-dio-nop-detailed-paths$sufix-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed_paths -e run_number="$i" | tee "$LOGS_DIR/t05-dio-file-detailed-paths$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths -e run_number="$i" | tee "$LOGS_DIR/t05-dio-elk-detailed-paths$sufix-"$i".txt" ;


        # ---- T6 - DETAILED-PATHS-UHASH
        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed_paths_uhash -e run_number="$i" | tee "$LOGS_DIR/t06-dio-nop-detailed-paths-uhash$sufix-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed_paths_uhash -e run_number="$i" | tee "$LOGS_DIR/t06-dio-file-detailed-paths-uhash$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_uhash -e run_number="$i" | tee "$LOGS_DIR/t06-dio-elk-detailed-paths-uhash$sufix-"$i".txt" ;


        # ---- T7 - DETAILED-PATHS-KHASH
        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed_paths_khash -e run_number="$i" | tee "$LOGS_DIR/t07-dio-nop-detailed-paths-khash$sufix-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed_paths_khash -e run_number="$i" | tee "$LOGS_DIR/t07-dio-file-detailed-paths-khash$sufix-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_khash -e run_number="$i" | tee "$LOGS_DIR/t07-dio-elk-detailed-paths-khash$sufix-"$i".txt" ;


    done
}

function filters {
    mkdir -p $LOGS_DIR

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        # ---- T8 - DETAILED-PATHS-FILTER-TID
        echo "Filebench - DIO - filter tid - Run $i"

        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed_paths_filter_tid -e run_number="$i" | tee "$LOGS_DIR/t07_dio-nop-detailed-paths-filter-tid-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed_paths_filter_tid -e run_number="$i" | tee "$LOGS_DIR/t07_dio-file-detailed-paths-filter-tid-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_filter_tid -e run_number="$i" | tee "$LOGS_DIR/t07_dio-elk-detailed-paths-filter-tid-"$i".txt" ;

        # ---- T9 - DETAILED-PATHS-FILTER-ORWC
        echo "Filebench - DIO - filter orwc - Run $i"

        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed_paths_filter_orwc -e run_number="$i" | tee "$LOGS_DIR/t08_dio-nop-detailed-paths-filter-orwc-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed_paths_filter_orwc -e run_number="$i" | tee "$LOGS_DIR/t08_dio-file-detailed-paths-filter-orwc-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_filter_orwc -e run_number="$i" | tee "$LOGS_DIR/t08_dio-elk-detailed-paths-filter-orwc-"$i".txt" ;

        # ---- T10 - DETAILED-PATHS-FILTER-READ
        echo "Filebench - DIO - filter read - Run $i"

        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed_paths_filter_read -e run_number="$i" | tee "$LOGS_DIR/t09_dio-nop-detailed-paths-filter-read-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed_paths_filter_read -e run_number="$i" | tee "$LOGS_DIR/t09_dio-file-detailed-paths-filter-read-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_filter_read -e run_number="$i" | tee "$LOGS_DIR/t09_dio-elk-detailed-paths-filter-read-"$i".txt" ;

        # ---- T11 - DETAILED-PATHS-FILTER-STAT
        echo "Filebench - DIO - filter stat - Run $i"

        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed_paths_filter_stat -e run_number="$i" | tee "$LOGS_DIR/t10_dio-nop-detailed-paths-filter-stat-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed_paths_filter_stat -e run_number="$i" | tee "$LOGS_DIR/t10_dio-file-detailed-paths-filter-stat-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_filter_stat -e run_number="$i" | tee "$LOGS_DIR/t10_dio-elk-detailed-paths-filter-stat-"$i".txt" ;

        # ---- T12 - DETAILED-PATHS-FILTER-RENAMEAT2
        echo "Filebench - DIO - filter rename - Run $i"

        # NOP
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_nop_detailed_paths_filter_renameat2 -e run_number="$i" | tee "$LOGS_DIR/t11_dio-nop-detailed-paths-filter-renameat2-"$i".txt" ;

        # FILE
        reset_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_file_detailed_paths_filter_renameat2 -e run_number="$i" | tee "$LOGS_DIR/t11_dio-file-detailed-paths-filter-renameat2-"$i".txt" ;

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_filter_renameat2 -e run_number="$i" | tee "$LOGS_DIR/t11_dio-elk-detailed-paths-filter-renameat2-"$i".txt" ;
    done
}


function ringbuf {
    mkdir -p $LOGS_DIR

    for ((i=$STARTING_RUN; i <= $RUNS; i++)); do
        # # ---- T13 - DETAILED-PATHS-RB1KB
        # echo "Filebench - DIO - RB 1KB - Run $i"

        # # ELK
        # setup_kube_cluster
        # ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_rb1kb -e run_number="$i" | tee "$LOGS_DIR/t13_dio-elk-detailed-paths-rb1kb-"$i".txt" ;

        # # ---- T14 - DETAILED-PATHS-RB16KB
        # echo "Filebench - DIO - RB 16KB - Run $i"

        # # ELK
        # setup_kube_cluster
        # ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_rb16kb -e run_number="$i" | tee "$LOGS_DIR/t14_dio-elk-detailed-paths-rb16kb-"$i".txt" ;

        # # ---- T15 - DETAILED-PATHS-RB32KB
        # echo "Filebench - DIO - RB 32KB - Run $i"

        # # ELK
        # setup_kube_cluster
        # ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_rb32kb -e run_number="$i" | tee "$LOGS_DIR/t15_dio-elk-detailed-paths-rb32kb-"$i".txt" ;

        # # ---- T16 - DETAILED-PATHS-RB64KB
        # echo "Filebench - DIO - RB 64KB - Run $i"

        # # ELK
        # setup_kube_cluster
        # ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_rb64kb -e run_number="$i" | tee "$LOGS_DIR/t16_dio-elk-detailed-paths-rb64kb-"$i".txt" ;

        # # ---- T17 - DETAILED-PATHS-RB128KB
        # echo "Filebench - DIO - RB 128KB - Run $i"

        # # ELK
        # setup_kube_cluster
        # ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_rb128kb -e run_number="$i" | tee "$LOGS_DIR/t17_dio-elk-detailed-paths-rb128kb-"$i".txt" ;

        # ---- T18 - DETAILED-PATHS-RB256KB
        echo "Filebench - DIO - RB 256KB - Run $i"

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_rb256kb -e run_number="$i" | tee "$LOGS_DIR/t18_dio-elk-detailed-paths-rb256kb-"$i".txt" ;

        # ---- T19 - DETAILED-PATHS-RB512KB
        echo "Filebench - DIO - RB 512KB - Run $i"

        # ELK
        setup_kube_cluster
        ansible-playbook -u gsd -i hosts.ini filebench_playbook.yml --tags dio_elk_detailed_paths_rb512kb -e run_number="$i" | tee "$LOGS_DIR/t19_dio-elk-detailed-paths-rb512kb-"$i".txt" ;
    done
}

"$@"
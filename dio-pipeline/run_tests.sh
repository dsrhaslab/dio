#!/bin/bash

LOGS_DIR="final_test_results/ansible_logs"

mkdir -p $LOGS_DIR

STARTING_RUN=1
RUNS=3

# --------

function reset_kube_cluster {
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


"$@"
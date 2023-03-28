#!/bin/bash

function reset_cluster {
	echo "Reseting k8s cluster"
	ansible-playbook -u $(whoami) -i hosts.ini reset-site.yaml
}

function install_dio_pipeline {
	reset_cluster

	echo "Installing k8s..."
	ansible-playbook -u $(whoami) -i hosts.ini playbook.yml

	echo "Configuring DIO k8s cluster..."
	ansible-playbook -u $(whoami) -i hosts.ini dio_playbook.yml --tags prepare_setup

	echo "Installing DIO pipeline..."
	ansible-playbook -u $(whoami) -i hosts.ini dio_playbook.yml --tags deploy_dio -e run_all=true
}

"$@"
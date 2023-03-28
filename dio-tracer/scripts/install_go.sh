#!/usr/bin/env bash

# -------

# $1: package name
function verify_package {
    PACKAGE_INSTALLED=$(dpkg-query -W -f='${Status}' $1 2>/dev/null | grep -c "ok installed")
    if [ $PACKAGE_INSTALLED -eq 0 ]; then
        sudo apt update -y
        sudo apt install -y $1
    fi
}

# -------


function install_go {

    echo -e "\n-----------------------------------\n"
    echo -e "--> Installing GO:"
    echo -e "\n-----------------------------------\n"

    # Install wget if not already installed
    verify_package "wget"

    CUR_DIR=$(pwd)
    # sudo apt-get install build-essential python3-pip -y
    wget https://go.dev/dl/go1.17.4.linux-amd64.tar.gz
    sudo rm -rf /usr/local/go
    sudo tar -C /usr/local -xzf go1.17.4.linux-amd64.tar.gz
    echo "export PATH=$PATH:/usr/local/go/bin" >> ~/.bashrc
    export PATH=$PATH:/usr/local/go/bin
    go version
    rm go1.17.4.linux-amd64.tar.gz
    cd $CUR_DIR
    echo -e "\n>>> Run 'source ~/.bashrc' to apply the changes"
}


"$@"


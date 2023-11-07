#!/usr/bin/env bash

function install_go {

    echo -e "\n-----------------------------------\n"
    echo -e "--> Installing GO:"
    echo -e "\n-----------------------------------\n"

    # Install dependencies
    sudo apt install -y software-properties-common wget

    CUR_DIR=$(pwd)
    wget https://go.dev/dl/go1.17.4.linux-amd64.tar.gz
    sudo rm -rf /usr/local/go
    sudo tar -C /usr/local -xzf go1.17.4.linux-amd64.tar.gz
    echo "export PATH=$PATH:/usr/local/go/bin" >> ~/.bashrc
    export PATH=$PATH:/usr/local/go/bin
    go version
    rm go1.17.4.linux-amd64.tar.gz
    cd $CUR_DIR
}


function install_bcc {

    echo -e "\n-----------------------------------\n"
    echo -e "--> Installing BCC:"
    echo -e "\n-----------------------------------\n"

    # Install dependencies
    sudo apt install -y linux-headers-generic
    sudo apt install -y software-properties-common
    sudo apt install -y wget python3 python3-pip
    sudo apt install -y bison build-essential cmake flex git libedit-dev libllvm9 llvm-9-dev libclang-9-dev python zlib1g-dev libelf-dev libfl-dev python3-distutils


    # Install and compile BCC
    git clone https://github.com/iovisor/bcc.git
    cd bcc
    git checkout 1313fd6a5e007ca795ea28363cb73f509728175a
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc)
    sudo make install
    cmake -DPYTHON_CMD=python3 ..
    pushd src/python/
    make -j$(nproc)
    sudo make install
    popd
}


if [ ! -z $1 ]
then
    if [[ "$1" == "go" ]]; then
        install_go
        echo -e "\n>>> Run 'source ~/.bashrc' to apply the changes"
    elif [[ "$1" == "bcc" ]]; then
        install_bcc
    elif [[ "$1" == "all" ]]; then
        install_go
        install_bcc
        echo -e "\n>>> Run 'source ~/.bashrc' to apply the changes"
    else
        echo "Unknown option. Supported options are 'go', 'bcc' or 'all'"
    fi
else
    install_go
    install_bcc
    echo -e "\n>>> Run 'source ~/.bashrc' to apply the changes"
fi
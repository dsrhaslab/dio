#!/usr/bin/env bash

REQUIRED_KERNEL_VERSION=5.4
CURRENT_KERNEL_VERSION=$(uname -r | cut -c1-3)

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


function upgrade_kernel_version {
    echo -e "\n-----------------------------------\n"
    echo -e "--> Checking kernel version:"
    echo -e "\n-----------------------------------\n"

    # Install bc if not already installed
    verify_package "bc"

    if (( $(echo "$CURRENT_KERNEL_VERSION < $REQUIRED_KERNEL_VERSION" | bc -l) )); then
        echo "Kernel version: $CURRENT_KERNEL_VERSION < Version required: $REQUIRED_KERNEL_VERSION "
        echo "Do you wish to install kernel version $REQUIRED_KERNEL_VERSION?"
        select yn in "Yes" "No"; do
            case $yn in
                Yes )
                    install_kernel_version
                    break
                    ;;
                No )
                    exit
                    ;;
            esac
        done
        echo "done"
    else
        echo "Kernel version ok! Required version is: $REQUIRED_KERNEL_VERSION and current kernel version is $CURRENT_KERNEL_VERSION. "
    fi
}

function install_kernel_version {
    echo -e "\n-----------------------------------\n"
    echo -e "--> Instaling kernel version $REQUIRED_KERNEL_VERSION:"
    echo -e "\n-----------------------------------\n"

    # Install wget if not already installed
    verify_package "wget"

    CUR_DIR=$(pwd)
    mkdir -p "kernel$REQUIRED_KERNEL_VERSION" && cd "kernel$REQUIRED_KERNEL_VERSION"
    wget -c https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.4/linux-headers-5.4.0-050400_5.4.0-050400.201911242031_all.deb
    wget -c https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.4/linux-headers-5.4.0-050400-generic_5.4.0-050400.201911242031_amd64.deb
    wget -c https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.4/linux-image-unsigned-5.4.0-050400-generic_5.4.0-050400.201911242031_amd64.deb
    wget -c https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.4/linux-modules-5.4.0-050400-generic_5.4.0-050400.201911242031_amd64.deb


    sudo dpkg -i *.deb

    cd $CUR_DIR
    rm -r "kernel$REQUIRED_KERNEL_VERSION"

    echo "Reboot required! Do you wish to reboot now?"
    select yn in "Yes" "No"; do
        case $yn in
            Yes )
                sudo reboot
                break
                ;;
            No )
                exit
                ;;
        esac
    done
}


"$@"


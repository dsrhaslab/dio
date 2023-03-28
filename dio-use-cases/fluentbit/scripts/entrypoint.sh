#!/bin/bash

set -e
set -m

FLUENT_PID=0
CLIENT_PID=0

function mount_new_filesystem {
    echo "Mounting new filesystem"
    dd if=/dev/zero of=disk.img bs=1M count=128
    mkfs.ext4 disk.img
    mkdir -p /fluent-bit/tests/mnt
    mount -o loop disk.img /fluent-bit/tests/mnt
}

function start_fluent_bit {
    ./start_fb.sh &
    sleep 2
    FLUENT_PID=$(cat fluent-bit.pid)
}

function start_client_app {
    ./app &
    sleep 2
    CLIENT_PID=$(cat client-app.pid)
}


mount_new_filesystem
start_fluent_bit
start_client_app

if [ $# -eq 0 ]; then
    echo "Starting Fluent Bit (vanilla mode)"

    # Initiating fluent-bit
    echo "start" > fb.fifo
    sleep 5

    # Initiating client app
    kill -SIGUSR1 $CLIENT_PID

    # Waiting for client app to finish
    wait $CLIENT_PID

    # Stopping fluent-bit
    sleep 20
    echo "stop" > fb.fifo
    wait $FLUENT_PID

else
    if [ $1 == "dio" ]; then

        # Initiating DIO
        export DIO_OPTIONS="--pid $FLUENT_PID,$CLIENT_PID "
        /fluent-bit/start_dio.sh &
        DIO_PID=$!
        sleep 20

        if ! [ -n "$(ps -p $DIO_PID -o pid=)" ]; then
            exit 1
        fi

        echo "PID of DIO: $DIO_PID"

        # Initiating fluent-bit
        echo -e "start\n" > fb.fifo
        sleep 5

        # Initiating client app
        kill -SIGUSR1 $CLIENT_PID

        # Waiting for client app to finish
        wait $CLIENT_PID

        # Stopping fluent-bit
        sleep 20
        echo "After client app finished. Stop fluent-bit"
        echo \e "stop\n" > fb.fifo
        echo "Waiting for fluent-bit to finish"
        wait $FLUENT_PID
        echo "Waiting for DIO to finish"
        wait $DIO_PID

    elif [ $1 == "strace" ]; then
        echo "Starting Fluent Bit (strace mode)"

        # Initiating strace
        mkdir -p /strace_data
        strace -yy -f -tt -s 0 -e trace=open,openat,creat,read,pread64,readv,write,pwrite64,writev,lseek,truncate,ftruncate,rename,renameat,renameat2,close,unlink,unlinkat,stat,fstat,lstat,fstatfs,newfstatat,setxattr,getxattr,listxattr,removexattr,lsetxattr,lgetxattr,llistxattr,lremovexattr,fsetxattr,fgetxattr,flistxattr,fsync,fdatasync,readahead,readlink,readlinkat,mknod,mknodat -o /strace_data/strace.out -p "$FLUENT_PID,$CLIENT_PID" &
        STRACE_PID=$!
        sleep 5

        # Initiating fluent-bit
        echo "start" > fb.fifo
        sleep 5

        # Initiating client app
        kill -SIGUSR1 $CLIENT_PID

        # Waiting for client app to finish
        wait $CLIENT_PID

        # Stopping fluent-bit
        sleep 30
        echo "stop" > fb.fifo
        wait $FLUENT_PID
        wait $STRACE_PID

    else
        echo "Unknown command: $1"
        echo "\tRun without arguments to start Elasticsearch (vanilla mode)"
        echo "\tRun with 'dio' to start Elasticsearch with DIO"
        echo "\tRun with 'strace' to start Elasticsearch with strace"
        exit 1
    fi
fi

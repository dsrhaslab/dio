#!/usr/bin/env bash

set -e
set -m

ES_COMMAND="bin/elasticsearch"

if [ $# -eq 0 ]; then
    echo "Starting Elasticsearch (vanilla mode)"
    runuser -u elasticsearch -- $ES_COMMAND
else
    if [ $1 == "dio" ]; then
        export DIO_OPTIONS="--user elasticsearch ${@:2}"
        exec /usr/share/dio/start_dio.sh $ES_COMMAND
    elif [ $1 == "strace" ]; then
        echo "Starting Elasticsearch (strace mode)"
        mkdir -p /strace_data
        chown -R elasticsearch:elasticsearch /strace_data
        runuser -u elasticsearch -- strace -yy -f -tt -s 0 -e trace=open,openat,creat,read,pread64,readv,write,pwrite64,writev,lseek,truncate,ftruncate,rename,renameat,renameat2,close,unlink,unlinkat,stat,fstat,lstat,fstatfs,newfstatat,setxattr,getxattr,listxattr,removexattr,lsetxattr,lgetxattr,llistxattr,lremovexattr,fsetxattr,fgetxattr,flistxattr,fsync,fdatasync,readahead,readlink,readlinkat,mknod,mknodat -o /strace_data/strace.out -- $ES_COMMAND
    else
        echo "Unknown command: $1"
        echo "\tRun without arguments to start Elasticsearch (vanilla mode)"
        echo "\tRun with 'dio' to start Elasticsearch with DIO"
        echo "\tRun with 'strace' to start Elasticsearch with strace"
        exit 1
    fi
fi
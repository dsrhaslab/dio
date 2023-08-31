#!/bin/bash

set -e
set -m

REDIS_SERVER_COMMAND="/redis/redis/src/redis-server /redis/redis/redis.conf"
REDIS_BENCHMARK_COMMAND="/redis/redis/src/redis-benchmark -h localhost -q -n 5000000 -r 5000000"

if [ $# -eq 0 ]; then
    echo "Starting redis-server (vanilla mode)"
    $REDIS_SERVER_COMMAND
else
    if [ $1 == "dio" ]; then
        exec /usr/share/dio/start_dio.sh $REDIS_SERVER_COMMAND
    elif [ $1 == "strace" ]; then
        echo "Starting redis-server (strace mode)"
        mkdir -p /strace_data
        exec strace -yy -f -tt -s 0 -e trace=open,openat,creat,read,pread64,readv,write,pwrite64,writev,lseek,truncate,ftruncate,rename,renameat,renameat2,close,unlink,unlinkat,stat,fstat,lstat,fstatfs,newfstatat,setxattr,getxattr,listxattr,removexattr,lsetxattr,lgetxattr,llistxattr,lremovexattr,fsetxattr,fgetxattr,flistxattr,fsync,fdatasync,readahead,readlink,readlinkat,mknod,mknodat -o /strace_data/strace.out -- $REDIS_SERVER_COMMAND
    elif [ $1 == "benchmark" ]; then
        echo "Starting redis-benchmark"
        time $REDIS_BENCHMARK_COMMAND
    else
        echo "Unknown command: $1"
        echo "\tRun without arguments to start Redis (vanilla mode)"
        echo "\tRun with 'dio' to start Redis with DIO"
        echo "\tRun with 'strace' to start Redis with strace"
        exit 1
    fi
fi
FROM ubuntu:20.04

RUN mkdir -p /filebench
WORKDIR /filebench

RUN apt-get update -y && \
    apt-get install -y linux-headers-generic && \
    apt-get install -y software-properties-common && \
    apt-get install -y wget python3 python3-pip git && \
    apt-get install -y automake autoconf libtool bison flex && \
    apt-get install -y strace

RUN git clone https://github.com/filebench/filebench.git && \
    cd filebench && \
    git checkout 22620e6 && \
    libtoolize && \
    aclocal && \
    autoheader && \
    automake --add-missing && \
    autoconf && \
    ./configure && \
    make && \
    make install

ENTRYPOINT ["strace", "-f", "-ecreat,open,openat,close,ftruncate,fsync,fdatasync,fstat,fstatfs,fgetxattr,fsetxattr,fremovexattr,flistxattr,truncate,rename,readlink,stat,newfstatat,getxattr,lgetxattr,setxattr,lsetxattr,removexattr,lremovexattr,listxattr,llistxattr,mknod,mknodat,setrlimit,readahead,read,pread64,readv,write,pwrite64,writev", "-o", "strace.out", "--"]

CMD [ "filebench", "-f", "/filebench/workloads/fileserver.f"]
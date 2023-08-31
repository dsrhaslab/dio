FROM taniaesteves/dio-tracer:v1.0.1

WORKDIR /

RUN apt-get install -y automake autoconf libtool bison flex strace

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

WORKDIR /filebench

COPY config/dio-filebench.yaml /usr/share/dio/conf/config.yaml
COPY config/fileserver.f /filebench/workloads/fileserver.f

COPY scripts/trace_filebench_tid.sh ./trace_filebench_tid.sh
RUN chmod +x ./trace_filebench_tid.sh

COPY scripts/entrypoint.sh ./entrypoint.sh
RUN chmod +x entrypoint.sh

RUN mkdir -p /dio_data

ENV STRACE_OPTIONS="-yy -s 0 -e trace=open,openat,creat,read,pread64,readv,write,pwrite64,writev,lseek,truncate,ftruncate,rename,renameat,renameat2,close,unlink,unlinkat,stat,fstat,lstat,fstatfs,newfstatat,setxattr,getxattr,listxattr,removexattr,lsetxattr,lgetxattr,llistxattr,lremovexattr,fsetxattr,fgetxattr,flistxattr,fsync,fdatasync,readahead,readlink,readlinkat,mknod,mknodat"

ENTRYPOINT [ "/filebench/entrypoint.sh" ]
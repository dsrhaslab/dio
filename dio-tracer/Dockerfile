FROM ubuntu:20.04

# Create DIO installation folder
RUN mkdir -p /usr/share/dio /usr/share/dio/tools
WORKDIR /usr/share/dio/tools

# Install dependencies
RUN apt-get update -y && \
    apt-get install -y linux-headers-generic && \
    apt-get install -y software-properties-common && \
    apt-get install -y wget python3 python3-pip && \
    apt-get install -y bison build-essential cmake flex git libedit-dev libllvm9 llvm-9-dev libclang-9-dev python zlib1g-dev libelf-dev libfl-dev python3-distutils && \
    apt-get install -y luajit luajit-5.1-dev && \
    apt-get install -y arping netperf iperf3

# Install GO-1.17.4
RUN wget https://go.dev/dl/go1.17.4.linux-amd64.tar.gz && \
    tar -xzf go1.17.4.linux-amd64.tar.gz && rm go1.17.4.linux-amd64.tar.gz

# Add go path to PATH ENV
ENV PATH=$PATH:/usr/share/dio/tools/go/bin

# Install BCC
RUN git clone https://github.com/iovisor/bcc.git && \
    cd bcc && \
    git checkout 1313fd6a5e007ca795ea28363cb73f509728175a && \
    mkdir -p build && \
    cd build && \
    cmake .. && \
    make && \
    make install && \
    cmake -DPYTHON_CMD=python3 .. && \
    cd build/src/python/ && \
    make -j$(nproc); make install;

WORKDIR /usr/share/dio
RUN mkdir -p bin conf
RUN mkdir -p /tmp/dio /dio_data

RUN apt-get install -y vim

# Copy DIO Binary
COPY bin/dio ./bin/

# Copy DIO default configuration file
COPY pkg/config/config.yaml ./conf/config.yaml

RUN apt-get install -y inotify-tools jq
RUN apt-get update -y && apt-get install -y apt-utils curl

COPY scripts/correlate_fp.sh /usr/share/dio/bin/fpca.sh
RUN chmod u+x /usr/share/dio/bin/fpca.sh

COPY scripts/entrypoint_dio.sh ./start_dio.sh
RUN chmod u+x start_dio.sh

ENV CORRELATE_PATHS false
ENV SLEEP_TIME 30
ENV N_TRIES 3

ENTRYPOINT [ "/usr/share/dio/start_dio.sh" ]
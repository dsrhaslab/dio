#!/bin/bash

TAG=${TAG:-"v1.0.2"}
IMAGE=${IMAGE:-"taniaesteves/dio-tracer"}
IMAGE_NAME="${IMAGE}:${TAG}"

function go_build {
    BUILD_COMMAND="go build -o bin/dio -ldflags=\"-X 'main.Version=${TAG}'\" ./cmd/dio-tracer"

    echo ">> Building dio-tracer binary..."
    echo "\$ $BUILD_COMMAND"

    eval $BUILD_COMMAND
    RESULT=$?
    if [ $RESULT -ne 0 ]; then exit $RESULT; fi

    CUR_USER=$(whoami)
    sudo mkdir -p /usr/share/dio/conf
    sudo chown -R $CUR_USER:$CUR_USER /usr/share/dio/conf
    cp pkg/config/config.yaml /usr/share/dio/conf/config.yaml
    echo ">>> Created binary 'bin/dio'"
}

function docker {
    DOCKER_BUILD_COMMAND="docker build -f Dockerfile . -t ${IMAGE_NAME}"

    echo ">> Building dio-tracer docker image..."
    echo "\$ $DOCKER_BUILD_COMMAND"

    eval "env $DOCKER_BUILD_COMMAND"
    RESULT=$?
    if [ $RESULT -ne 0 ]; then exit $RESULT; fi

    DOCKER_RUN="docker run --name dio --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v /home/gsd/dio_data:/dio_data -v /home/gsd/dio.yaml:/usr/share/dio/conf/config.yaml -e CORRELATE_PATHS=true -e ES_URL=localhost:31111 -e SLEEP_TIME=5 ${IMAGE_NAME} ls"
    echo ">> Example of how to run DIO docker container. 'ls' is the command to be observed:"
    echo "\t\$ $DOCKER_RUN"

}

if [ ! -z $1 ]
then
    if [[ "$1" == "go" ]]; then
        go_build
    elif [[ "$1" == "docker" ]]; then
        docker
    elif [[ "$1" == "all" ]]; then
        go_build
        docker
    else
        echo "Unknown option. Supported options are 'go', 'docker' or 'all'"
    fi
else
    go_build
fi
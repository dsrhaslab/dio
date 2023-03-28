# DIO Tracer

eBPF-based tracer that non-intrusively intercepts applications' I/O system calls, enriches collected data with extra context from the kernel, and sends the data to DIO's analysis pipeline.

## How to build

#### Build Go binary:
```
bash scripts/build.sh go
```

#### Build Docker image:
```
bash scripts/build.sh docker
```

#### Build Go binary and Docker image:
```
bash scripts/build.sh all
```

## How to run

### From binary:
```
sudo bin/dio-tracer [options] -- <COMMAND>
```

#### Example:
```
sudo bin/dio-tracer --config config.yaml -- ls
```

### From Docker:
```
docker run --name <container_name> --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v /home/gsd/dio_data:/dio_data -v <local_config_file>:/usr/share/dio/conf/config.yaml taniaesteves/dio-tracer:<TAG> <COMMAND>
```

#### Run with File path correlation algorithm on
```
docker run --name <container_name> --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v /home/gsd/dio_data:/dio_data -v <local_config_file>:/usr/share/dio/conf/config.yaml -e CORRELATE_PATHS=true -e ES_SERVERS=localhost:31111 -e SLEEP_TIME=5 taniaesteves/dio-tracer:<TAG> <COMMAND>
```

#### Example:
```
docker run --name dio --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v /home/gsd/dio_data:/dio_data -v /home/gsd/dio.yaml:/usr/share/dio/conf/config.yaml -e CORRELATE_PATHS=true -e ES_SERVERS=localhost:31111 -e SLEEP_TIME=5 taniaesteves/dio-tracer:v1.0.1 ls
```

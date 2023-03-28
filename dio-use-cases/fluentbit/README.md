# Fluent Bit use case

This use case considers the diagnosis of a data loss issue identified in the Fluent Bit application ([#1875](https://github.com/fluent/fluent-bit/issues/1875),[#4895](https://github.com/fluent/fluent-bit/issues/4895)).

The instructions to reproduce the use case are available at [how to reproduce](https://dio-tool.netlify.app/use-cases/fluentbit/how2run), while an extended set of visualizations provided by DIO is available in the [portfolio](https://dio-tool.netlify.app/use-cases/fluentbit/portfolio).


## Docker images

### Build images

##### Fluent Bit (v1.4.0)
```
docker build -t dio-fluentbit:v1.4.0 -f Dockerfile-flb-v1.4.0 .
```

##### Fluent Bit (v2.0.5)
```
docker build -t dio-fluentbit:v2.0.5 -f Dockerfile-flb-v2.0.5 .
```

Publicly available at: [https://hub.docker.com/r/taniaesteves/dio-fluentbit](https://hub.docker.com/r/taniaesteves/dio-fluentbit)

### Pull images
##### Fluent Bit (v1.4.0)
```
docker pull taniaesteves/dio-fluentbit:v1.4.0
```

##### Fluent Bit (v2.0.5)
```
docker pull taniaesteves/dio-fluentbit:v2.0.5
```
## Run experiments

#### Vanilla (Fluent Bit v1.4.0)
```
docker run -it --rm --name fluentbit --pid=host --net=host \
  --privileged --cap-add=ALL -v /lib/modules:/lib/modules \
  -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ \
  taniaesteves/dio-fluentbit:v1.4.0
```

#### Strace (Fluent Bit v1.4.0)
```
docker run -it --rm --name fluentbit --pid=host --net=host \
  --privileged --cap-add=ALL  -v /lib/modules:/lib/modules \
  -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ \
  -v /tmp/strace_data:/strace_data \
  taniaesteves/dio-fluentbit:v1.4.0 strace
```

#### DIO (Fluent Bit v1.4.0)
```
docker run -it --rm --name fluentbit  --pid=host --net=host \
  --privileged --cap-add=ALL  -v /lib/modules:/lib/modules \
  -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ \
  -v /tmp/dio_data:/dio_data -e CORRELATE_PATHS=true \
  -e ES_SERVERS=http://<DIO_ES_URL>:<DIO_ES_PORT> \
  taniaesteves/dio-fluentbit:v1.4.0 dio
```

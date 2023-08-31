# Elasticsearch use case

## Build image
```

```
*Requires the download of elasticsearch-8.3.0-SNAPSHOT-linux-x86_64.tar.gz*

## Initiate Elasticsearch:

#### Vanilla version
```
docker run -it --rm --name es830  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ taniaesteves/elasticsearch_dio:latest
```

#### Strace version
```
docker run -it --rm --name es830  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v /home/gsd/strace_data:/strace_data taniaesteves/elasticsearch_dio:latest strace
```

#### DIO version
```
docker run -it --rm --name es830  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v /home/gsd/dio_data:/dio_data -v /home/gsd/dio.yaml:/usr/share/dio/conf/config.yaml -v /tmp/dio/:/tmp/dio/ -e CORRELATE_PATHS=true -e ES_URL=cloud124:31111  taniaesteves/elasticsearch_dio:latest dio --target_paths /usr/share/elasticsearch-8.3.0-SNAPSHOT
```

## Run benchmark:
```
docker run --rm  --net=host elastic/rally race --track=geonames  --pipeline=benchmark-only --target-hosts=localhost:9200
```




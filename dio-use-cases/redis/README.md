# Redis use case

## Docker images

- taniaesteves/redis_dio:v1 - old version of Redis
- taniaesteves/redis_dio:v2 - new version of Redis (PR#9934)

## Initiate Redis:

#### Vanilla version
```
docker run -it --rm --name redis-server  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ taniaesteves/redis_dio:v1
```

#### Strace version
```
docker run -it --rm --name redis-server  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v /home/gsd/strace_data:/strace_data taniaesteves/redis_dio:v1 strace
```

#### DIO version
```
docker run -it --rm --name redis-server  --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ -v /home/gsd/dio_data:/dio_data -v /home/gsd/dio.yaml:/usr/share/dio/conf/config.yaml -e CORRELATE_PATHS=true -e ES_URL=cloud124:31111  taniaesteves/redis_dio:v1 dio
```

## Run benchmark:
```
docker run -it --rm --name redis-bench --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ taniaesteves/redis_dio:v1 benchmark
```




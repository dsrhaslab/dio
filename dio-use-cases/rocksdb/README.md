# RocksDB use case

This use case aims at identifying the root cause for high tail latency at client requests issued to RocksDB (reported in [SILK](https://www.usenix.org/system/files/atc19-balmau.pdf) paper).

The instructions to reproduce the use case are available at [how to reproduce](https://dio-tool.netlify.app/use-cases/rocksdb/how2run), while an extended set of visualizations provided by DIO is available in the [portfolio](https://dio-tool.netlify.app/use-cases/rocksdb/portfolio).


## Docker image


### Build image

##### RocksDB (v5.17.2)
```
docker build -t dio-rocksdb:v5.17.2 -f Dockerfile-rocksdb-v5.17.2 .
```

Publicly available at: [https://hub.docker.com/r/taniaesteves/dio-rocksdb](https://hub.docker.com/r/taniaesteves/dio-rocksdb)

### Pull images
##### RocksDB (v5.17.2)
```
docker pull taniaesteves/dio-rocksdb:v5.17.2
```

## Run experiments

### Load
```
docker run -it --rm --name rocksdb --pid=host --net=host \
  --privileged --cap-add=ALL -v /lib/modules:/lib/modules \
  -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ \
  -v /tmp/rocksdb_data/kvstore:/rocksdb/test/kvstore \
  -v /tmp/rocksdb_data/results:/rocksdb/test/results \
  -e ROCKSDB_LOAD=100000000 -e DB_BENCH_OPS=100000000 \
  taniaesteves/dio-rocksdb:v5.17.2 load
```

### YCSB WA

#### Vanilla
```
docker run -it --rm --name rocksdb --pid=host --net=host \
  --privileged --cap-add=ALL -v /lib/modules:/lib/modules \
  -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ \
  -v /tmp/rocksdb_data/kvstore:/rocksdb/test/kvstore \
  -v /tmp/rocksdb_data/results:/rocksdb/test/results \
  -e ROCKSDB_LOAD=100000000 -e DB_BENCH_OPS=100000000 \
  taniaesteves/rocksdb:v5.17.2 ycsbwa
```

#### Strace
```
docker run -it --rm --name rocksdb  --pid=host --net=host \
  --privileged --cap-add=ALL  -v /lib/modules:/lib/modules \
  -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ \
  -v /tmp/strace_data:/strace_data \
  -v /tmp/rocksdb_data/kvstore:/rocksdb/test/kvstore \
  -v /tmp/rocksdb_data/results:/rocksdb/test/results \
  -e ROCKSDB_LOAD=100000000 -e DB_BENCH_OPS=100000000 \
  taniaesteves/rocksdb:v5.17.2 strace ycsbwa
```

#### DIO
```
docker run -it --rm --name rocksdb  --pid=host --net=host \
  --privileged --cap-add=ALL -v /lib/modules:/lib/modules \
  -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ \
  -v /tmp/dio_data:/dio_data \
  -v /tmp/dio.yaml:/usr/share/dio/conf/config.yaml \
  -v /tmp/rocksdb_data/kvstore:/rocksdb/test/kvstore \
  -v /tmp/rocksdb_data/results:/rocksdb/test/results \
  -e ROCKSDB_LOAD=100000000 -e DB_BENCH_OPS=100000000 \
  taniaesteves/rocksdb:v5.17.2 dio ycsbwa
```


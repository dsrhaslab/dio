# DIO Scripts

## install_dio_dependencies.sh

Installs DIO's dependencies.

**Usage:**
```
bash install_dio_dependencies.sh <option>
```

**Option:**
- `go` - installs Go (v1.17.4).
- `bcc` - installs BCC (#1313fd6a5).
- `all`: installs both Go and BCC.

<hr>

## build.sh
Builds dio-tracer binary and docker image.

**Usage:**
```
bash build.sh <option>
```

**Option:**
- `go` - builds dio-tracer binary (locally).
- `docker` - builds dio-tracer docker image.
- `all`: builds both dio-tracer binary and docker image.

<hr>

## install_kernel_v5.4.sh
Verifies kernel version and installs version 5.4 (if needed).

**Usage:**
```
bash install_kernel_v5.4.sh upgrade_kernel_version
```
<hr>

## entrypoint_dio.sh
Entrypoint script for dio-tracer docker image.

**Usage:**
```
bash entrypoint_dio.sh
```

<hr>

## dio_exporter.py
Sends DIO's traces to Elasticsearch.

**Dependencies:**
- Python packages:
    - elasticsearch

**Usage:**
```
python3 dio_exporter.py <options> <file>
```

**Options:**
- `-u`, `--url`: IP and port of elasticsearch server (`<IP>:<PORT>`) (e.g., _localhost:9200_)
- `--session`: Session name reported by dio-tracer (e.g., _frkfts_09.11.2023-17.04.55_)
- `--size`: Bulk size (e.g., _1000_)
- `-d`, `--debug`: Enable debug mode (e.g., _false_)
- `<file>`: DIO's trace file (e.g., _dio-trace.json_).

<hr>

## correlate_fp.sh
Correlates file tags with file paths.

**Dependencies:**
- Linux commands:
    - jq
    - curl
    - inotify-tools

**Usage (Single execution):**
```
bash correlate_fp.sh correlate_index <elasticsearch_url> <session_name>
```

**Options:**
- `elasticsearch_url`: IP and port of elasticsearch server (`<IP>:<PORT>`) (e.g., _localhost:9200_)
- `session_name`: Session name reported by dio-tracer (e.g., _frkfts_09.11.2023-17.04.55_)

**Usage (Running as a daemon):**
```
bash correlate_fp.sh correlate_daemon <elasticsearch_url> <sleep_time> <n_tries> <stop_on_delete>
```
**Options:**
- `elasticsearch_url`: IP and port of elasticsearch server (`<IP>:<PORT>`) (e.g., _localhost:9200_)
- `session_name`: Session name reported by dio-tracer (e.g., _frkfts_09.11.2023-17.04.55_)
- `sleep_time`: Time (in seconds) to wait before trying to correlate a given index again (e.g., _30_).
- `n_tries`: Number of attempts to correlate a given index when there are no events to correlate (e.g., _3_).
- `stop_on_delete`: Whether to stop or not after correlating the first index (e.g., _false_).

<hr>
# DIO Scripts

## install_go.sh
Downloads and installs GO (1.17.4).

## update_kernel.sh
Verifies kernel version and installs version 5.4 (if needed).

## entrypoint_dio.sh
Entrypoint script for DIO's Docker image.

## build.sh
Builds DIO's tracer binary and docker image.

## dio_exporter.py
Sends DIO trace to Elasticsearch.

### Dependencies
##### Python packages:
- elasticsearch

## correlate_fp.sh
Correlates file tags with file paths.

### Dependencies
##### Linux commands:
- jq
- curl
- inotify

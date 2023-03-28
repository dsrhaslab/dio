#!/bin/bash

while true
do
    sleep 1200
    echo "`date` Truncating file $1. Current size is $(stat -c%s $1) bytes."
    truncate -s 0 $1
done
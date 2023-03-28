#!/bin/bash

set -e
set -m

rm -f exampledb

echo "fluent-bit | PID: $$"
echo $$ > fluent-bit.pid

mkfifo fb.fifo
echo "fluent-bit | Waiting signal SIGUSR1 to start..."
read < fb.fifo

/fluent-bit/build/bin/fluent-bit -c /fluent-bit/fluentbit.conf
echo "fluent-bit | Waiting signal SIGUSR1 to stop..."

read < fb.fifo
rm fb.fifo
echo "fluent-bit | Stopping..."

FB_PID=$(pidof fluent-bit)
kill -SIGINT $FB_PID

echo "fluent-bit | Stopped"

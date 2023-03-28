#!/bin/bash

mkdir -p /tmp/inputs

head -c 1G </dev/urandom >/tmp/inputs/inputA.txt
ls -lsh /tmp/inputs/inputA.txt


head -c 1G </dev/urandom >/tmp/inputs/inputB.txt
ls -lsh /tmp/inputs/inputB.txt

touch /tmp/inputs/outputB.txt
ls -lsh /tmp/inputs/outputB.txt

echo "test symlink" > /tmp/inputs/readlink.file
ln -sf /tmp/inputs/readlink.file /tmp/inputs/readlink.symlink
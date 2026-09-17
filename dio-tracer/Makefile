.PHONY: all clean
.DELETE_ON_ERROR:

all: build/dio

vmlinux.h:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h.tmp
	mv vmlinux.h.tmp vmlinux.h

build/dio.bpf.o: dio.bpf.c dio.h vmlinux.h
	mkdir -p build
	clang -O2 -g -target bpf -D__TARGET_ARCH_x86 -Wall -Werror -Wno-missing-declarations -I. -c dio.bpf.c -o build/dio.bpf.o

build/dio.skel.h: build/dio.bpf.o
	bpftool gen skeleton build/dio.bpf.o > build/dio.skel.h.tmp
	mv build/dio.skel.h.tmp build/dio.skel.h

build/dio: main.c config.c writer.c config.h writer.h dio.h build/dio.skel.h
	cc -O2 -g -std=c11 -Wall -Wextra -Werror -I. -Ibuild main.c config.c writer.c -o build/dio -lbpf -lyaml

clean:
	rm -rf build
	rm -f vmlinux.h vmlinux.h.tmp
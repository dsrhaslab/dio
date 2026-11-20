.PHONY: all clean
.DELETE_ON_ERROR:

SOURCES = main.c config.c loader.c consumer.c decoder.c filepaths.c writer.c stats.c
HEADERS = config.h loader.h consumer.h decoder.h filepaths.h writer.h stats.h dio.h

all: build/dio

vmlinux.h:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h.tmp
	mv vmlinux.h.tmp vmlinux.h

build/dio.bpf.o: dio.bpf.c dio.h vmlinux.h
	mkdir -p build
	clang -O2 -g -target bpf -D__TARGET_ARCH_x86 -Wall -Werror -Wno-missing-declarations -I. -I/usr/include/x86_64-linux-gnu -c dio.bpf.c -o build/dio.bpf.o

build/dio.skel.h: build/dio.bpf.o
	bpftool gen skeleton build/dio.bpf.o > build/dio.skel.h.tmp
	mv build/dio.skel.h.tmp build/dio.skel.h

build/dio: $(SOURCES) $(HEADERS) build/dio.skel.h
	cc -O2 -g -std=c11 -Wall -Wextra -Werror -I. -Ibuild $(SOURCES) -o build/dio -lbpf -lyaml -lyyjson

clean:
	rm -rf build
	rm -f vmlinux.h vmlinux.h.tmp

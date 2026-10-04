#ifndef DIO_WRITER_H
#define DIO_WRITER_H

#include "dio.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct dio_writer {
    FILE *output;
    int files_fd;
    int possible_cpus;
    struct dio_file_info *files;
    bool first;
    uint64_t epoch_offset_ns;
    char hostname[256];
    char session_name[128];
};

int dio_writer_open(struct dio_writer *writer, const char *filename, int files_fd, int possible_cpus, const char *session_name);
int dio_writer_record(void *context, void *data, size_t size);
int dio_writer_close(struct dio_writer *writer);

#endif

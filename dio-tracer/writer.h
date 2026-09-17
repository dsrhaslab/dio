#ifndef DIO_WRITER_H
#define DIO_WRITER_H

#include "dio.h"
#include <stddef.h>
#include <stdio.h>

struct dio_writer {
    FILE *output;
    int files_fd;
    int possible_cpus;
    struct dio_file_info *files;
};

int dio_writer_open(struct dio_writer *writer, const char *filename, int files_fd, int possible_cpus);
int dio_writer_record(void *context, void *data, size_t size);
int dio_writer_close(struct dio_writer *writer);

#endif

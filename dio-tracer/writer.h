#ifndef DIO_WRITER_H
#define DIO_WRITER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

union dio_record;
struct dio_resolved_path;

struct dio_writer {
    FILE *output;
    bool first;
    bool output_failed;
    uint64_t epoch_offset_ns;
    char hostname[256];
    char session_name[128];
};

int dio_writer_open(struct dio_writer *writer, const char *filename, const char *session_name);
int dio_writer_write(struct dio_writer *writer, const union dio_record *record,
                     const struct dio_resolved_path *path);
int dio_writer_close(struct dio_writer *writer);

#endif

#ifndef DIO_WRITER_H
#define DIO_WRITER_H

#include "dio.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct dio_user_stats {
    uint64_t received;
    uint64_t written;
    uint64_t stale;
    uint64_t rejected;
    uint64_t failed;
};

struct dio_writer {
    FILE *output;
    int paths_fd; // map path_slots
    int possible_cpus;
    struct dio_path_slot *path_values; // uma copia do slot consultado por CPU possivel
    bool first;
    bool show_stats;
    bool output_failed;
    struct dio_user_stats stats[DIO_EVENT_MAX];
    uint64_t unknown_received;
    uint64_t epoch_offset_ns;
    char hostname[256];
    char session_name[128];
};

int dio_writer_open(struct dio_writer *writer, const char *filename, int paths_fd, int possible_cpus, const char *session_name);
int dio_writer_record(void *context, void *data, size_t size);
int dio_writer_close(struct dio_writer *writer);

#endif

#ifndef DIO_FILEPATHS_H
#define DIO_FILEPATHS_H

#include "dio.h"

#define DIO_PATH_STALE 1

struct dio_filepaths {
    int paths_fd; // pertence ao loader
    int possible_cpus;
    struct dio_path_slot *path_values;
};

struct dio_resolved_path {
    char text[DIO_PATH_LEN + 32]; // espaço para :[inode] nos pipes e sockets
    __u16 file_type;
};

int dio_filepaths_open(struct dio_filepaths *paths, int paths_fd, int possible_cpus);
int dio_filepaths_resolve(struct dio_filepaths *paths, const struct dio_path_event *event,
                         struct dio_resolved_path *path);
void dio_filepaths_close(struct dio_filepaths *paths);

#endif

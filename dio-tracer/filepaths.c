#define _DEFAULT_SOURCE
#include "filepaths.h"

#include <bpf/bpf.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int dio_filepaths_open(struct dio_filepaths *paths, int paths_fd, int possible_cpus) {
    if (paths_fd < 0 || possible_cpus <= 0) {
        errno = EINVAL;
        return -1;
    }

    paths->paths_fd = paths_fd;
    paths->possible_cpus = possible_cpus;
    paths->path_values = calloc(possible_cpus, sizeof(*paths->path_values));
    return paths->path_values == NULL ? -1 : 0;
}

int dio_filepaths_resolve(struct dio_filepaths *paths, const struct dio_path_event *event,
                         struct dio_resolved_path *path) {
    if (event->cpu >= paths->possible_cpus || event->index >= DIO_PATH_SLOTS) {
        return -EINVAL;
    }

    if (bpf_map_lookup_elem(paths->paths_fd, &event->index, paths->path_values) != 0) {
        return -EIO;
    }

    const struct dio_path_slot *slot = &paths->path_values[event->cpu];
    if (event->generation == 0 || slot->generation != event->generation) {
        // este slot já mudou, o path que lá está pode ser de outro ficheiro
        return DIO_PATH_STALE;
    }

    if (slot->offset >= DIO_PATH_LEN || slot->size == 0 || slot->size > DIO_PATH_LEN - slot->offset) {
        return -EINVAL;
    }

    path->file_type = slot->file_type;
    size_t length = strnlen(slot->filename + slot->offset, slot->size);
    memcpy(path->text, slot->filename + slot->offset, length);
    path->text[length] = '\0';

    if ((slot->file_type & S_IFMT) == S_IFSOCK ||
        ((slot->file_type & S_IFMT) == S_IFIFO && strcmp(path->text, "pipe") == 0)) {
        snprintf(path->text + length, sizeof(path->text) - length, ":[%u]", event->ino);
    }

    return 0;
}

void dio_filepaths_close(struct dio_filepaths *paths) {
    free(paths->path_values);
    paths->path_values = NULL;
}

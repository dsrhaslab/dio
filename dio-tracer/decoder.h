#ifndef DIO_DECODER_H
#define DIO_DECODER_H

#include "dio.h"
#include <stddef.h>
#include <stdint.h>

union dio_record {
    struct dio_header header;
    struct dio_open_event open;
    struct dio_data_event data;
    struct dio_fd_event fd;
    struct dio_seek_event seek;
    struct dio_path_event path;
};

// id fica a zero se o registo não tiver um tipo reconhecido.
int dio_decode(const void *data, size_t size, union dio_record *record, uint32_t *id);

#endif

#include "decoder.h"

#include <errno.h>
#include <string.h>

int dio_decode(const void *data, size_t size, union dio_record *record, uint32_t *id) {
    struct dio_header header;
    *id = 0;
    if (data == NULL || size < sizeof(header)) {
        return -EINVAL;
    }
    memcpy(&header, data, sizeof(header));

    size_t expected;
    switch (header.event_id) {
        case DIO_PATH: expected = sizeof(record->path); break;

        case DIO_OPEN:
        case DIO_CREAT:
        case DIO_OPENAT: expected = sizeof(record->open); break;

        case DIO_READ:
        case DIO_PREAD64:
        case DIO_READV:
        case DIO_WRITE:
        case DIO_PWRITE64:
        case DIO_WRITEV: expected = sizeof(record->data); break;

        case DIO_CLOSE:
        case DIO_FSYNC:
        case DIO_FDATASYNC: expected = sizeof(record->fd); break;

        case DIO_LSEEK: expected = sizeof(record->seek); break;
        default: return -EINVAL;
    }

    *id = header.event_id;
    if (size != expected) {
        return -EINVAL;
    }

    memcpy(record, data, expected);
    return 0;
}

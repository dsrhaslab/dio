#include "consumer.h"
#include "decoder.h"
#include "filepaths.h"
#include "stats.h"
#include "writer.h"

#include <bpf/libbpf.h>
#include <errno.h>

static int receive_record(void *context, void *data, size_t size) {
    struct dio_consumer *consumer = context;
    union dio_record record;
    uint32_t id;
    int result = dio_decode(data, size, &record, &id);
    dio_stats_receive(consumer->stats, id);

    if (result == 0) {
        if (consumer->writer->output_failed) {
            result = -EIO;
        }
        else if (id == DIO_PATH) {
            struct dio_resolved_path path;
            result = dio_filepaths_resolve(consumer->paths, &record.path, &path);
            if (result == 0) {
                result = dio_writer_write(consumer->writer, &record, &path);
            }
        }
        else {
            result = dio_writer_write(consumer->writer, &record, NULL);
        }
    }

    dio_stats_finish(consumer->stats, id, result);
    // um slot reutilizado só invalida este EventPath
    return result == DIO_PATH_STALE ? 0 : result;
}

int dio_consumer_open(struct dio_consumer *consumer, int events_fd, struct dio_filepaths *paths,
                      struct dio_writer *writer, struct dio_stats *stats) {
    consumer->paths = paths;
    consumer->writer = writer;
    consumer->stats = stats;
    consumer->ring = ring_buffer__new(events_fd, receive_record, consumer, NULL);
    return consumer->ring == NULL ? -1 : 0;
}

int dio_consumer_poll(struct dio_consumer *consumer, int timeout_ms) {
    return ring_buffer__poll(consumer->ring, timeout_ms);
}

int dio_consumer_drain(struct dio_consumer *consumer) {
    return ring_buffer__consume(consumer->ring);
}

void dio_consumer_close(struct dio_consumer *consumer) {
    ring_buffer__free(consumer->ring);
    consumer->ring = NULL;
}

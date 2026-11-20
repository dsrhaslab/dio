#ifndef DIO_CONSUMER_H
#define DIO_CONSUMER_H

struct ring_buffer;
struct dio_filepaths;
struct dio_writer;
struct dio_stats;

struct dio_consumer {
    struct ring_buffer *ring;
    struct dio_filepaths *paths;
    struct dio_writer *writer;
    struct dio_stats *stats;
};

int dio_consumer_open(struct dio_consumer *consumer, int events_fd, struct dio_filepaths *paths,
                      struct dio_writer *writer, struct dio_stats *stats);
int dio_consumer_poll(struct dio_consumer *consumer, int timeout_ms);
int dio_consumer_drain(struct dio_consumer *consumer);
void dio_consumer_close(struct dio_consumer *consumer);

#endif

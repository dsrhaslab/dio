#ifndef DIO_STATS_H
#define DIO_STATS_H

#include "dio.h"
#include <stdbool.h>
#include <stdint.h>

struct dio_bpf;

struct dio_user_stats {
    uint64_t received;
    uint64_t written;
    uint64_t stale;
    uint64_t rejected;
    uint64_t failed;
};

struct dio_stats {
    bool enabled;
    struct dio_user_stats events[DIO_EVENT_MAX];
    uint64_t unknown_received;
};

void dio_stats_receive(struct dio_stats *stats, uint32_t id);
void dio_stats_finish(struct dio_stats *stats, uint32_t id, int result);
int dio_stats_print(struct dio_bpf *skel, const struct dio_stats *stats, int cpus, bool drained, bool output_failed);

#endif

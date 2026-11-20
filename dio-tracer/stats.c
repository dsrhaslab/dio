#include "stats.h"
#include "filepaths.h"
#include "dio.skel.h"

#include <bpf/bpf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

void dio_stats_receive(struct dio_stats *stats, uint32_t id) {
    if (!stats->enabled) {
        return;
    }
    if (id == 0 || id >= DIO_EVENT_MAX) {
        stats->unknown_received++;
        return;
    }
    stats->events[id].received++;
}

void dio_stats_finish(struct dio_stats *stats, uint32_t id, int result) {
    if (!stats->enabled || id == 0 || id >= DIO_EVENT_MAX) {
        return;
    }

    struct dio_user_stats *event = &stats->events[id];
    if (result == 0) {
        event->written++;
    }
    else if (result == DIO_PATH_STALE) {
        event->stale++;
    }
    else if (result == -EINVAL) {
        event->rejected++;
    }
    else {
        event->failed++;
    }
}

static int pending_counts(int map_fd, uint64_t pending[DIO_EVENT_MAX]) {
    struct dio_key key;
    struct dio_key next;
    const struct dio_key *previous = NULL;
    while (bpf_map_get_next_key(map_fd, previous, &next) == 0) {
        if (next.event_id >= DIO_EVENT_MAX) {
            errno = EINVAL;
            return -1;
        }
        pending[next.event_id]++;
        key = next;
        previous = &key;
    }
    return errno == ENOENT ? 0 : -1;
}

static void add_stats(struct dio_kernel_stats *total, const struct dio_kernel_stats *cpu) {
    total->entered += cpu->entered;
    total->pending_failed += cpu->pending_failed;
    total->pending_replaced += cpu->pending_replaced;
    total->matched += cpu->matched;
    total->filtered += cpu->filtered;
    total->emitted += cpu->emitted;
    total->ring_lost += cpu->ring_lost;
    total->abandoned += cpu->abandoned;
    total->exit_without_entry += cpu->exit_without_entry;
    total->pending_delete_failed += cpu->pending_delete_failed;
    total->path_attempted += cpu->path_attempted;
    total->path_prepare_failed += cpu->path_prepare_failed;
    total->args_unavailable += cpu->args_unavailable;
}

int dio_stats_print(struct dio_bpf *skel, const struct dio_stats *stats, int cpus, bool drained, bool output_failed) {
    const char *names[DIO_EVENT_MAX] = {
        [DIO_CREAT] = "creat", [DIO_OPEN] = "open",
        [DIO_PATH] = "EventPath", [DIO_OPENAT] = "openat", [DIO_READ] = "read",
        [DIO_PREAD64] = "pread64", [DIO_READV] = "readv", [DIO_WRITE] = "write",
        [DIO_PWRITE64] = "pwrite64", [DIO_WRITEV] = "writev", [DIO_CLOSE] = "close",
        [DIO_FSYNC] = "fsync", [DIO_FDATASYNC] = "fdatasync", [DIO_LSEEK] = "lseek"
    };
    uint64_t pending[DIO_EVENT_MAX] = {0};
    if (pending_counts(bpf_map__fd(skel->maps.entry_syscall_args), pending) != 0 ||
        pending_counts(bpf_map__fd(skel->maps.entry_data_buf_args), pending) != 0 ||
        pending_counts(bpf_map__fd(skel->maps.entry_fd_args), pending) != 0 ||
        pending_counts(bpf_map__fd(skel->maps.entry_seek_args), pending) != 0) {
        perror("ler chamadas pendentes");
        return -1;
    }

    // o lookup devolve uma copia por CPU; arredonda cada tamanho para multiplo de 8
    size_t stride = (sizeof(struct dio_kernel_stats) + 7) & ~(size_t)7;
    unsigned char *values = calloc((size_t)cpus, stride);
    if (values == NULL) {
        perror("memoria para contadores");
        return -1;
    }
    bool all_balanced = stats->unknown_received == 0;
    for (uint32_t id = 0; id < DIO_EVENT_MAX; id++) {
        if (names[id] == NULL) {
            continue;
        }
        if (bpf_map_lookup_elem(bpf_map__fd(skel->maps.counters), &id, values) != 0) {
            perror("ler contadores");
            free(values);
            return -1;
        }
        struct dio_kernel_stats total = {0};
        for (int cpu = 0; cpu < cpus; cpu++) {
            add_stats(&total, (const struct dio_kernel_stats *)(values + (size_t)cpu * stride));
        }
        const struct dio_user_stats *user = &stats->events[id];
        bool balanced = user->received == user->written + user->stale + user->rejected + user->failed;
        if (drained && total.emitted != user->received) {
            balanced = false;
        }
        fprintf(stderr, "DIO stats: event=%s", names[id]);
        if (id == DIO_PATH) {
            balanced = balanced && total.path_attempted == total.path_prepare_failed + total.emitted + total.ring_lost;
            fprintf(stderr, " attempted=%" PRIu64 " prepare_failed=%" PRIu64,
                    (uint64_t)total.path_attempted, (uint64_t)total.path_prepare_failed);
        }
        else {
            uint64_t eligible = total.matched >= total.filtered ? total.matched - total.filtered : 0;
            balanced = balanced && total.entered == total.pending_failed + total.pending_replaced + total.matched + total.abandoned + pending[id];
            balanced = balanced && total.pending_delete_failed == 0 && total.matched >= total.filtered;
            balanced = balanced && eligible == total.emitted + total.ring_lost;
            fprintf(stderr, " entered=%" PRIu64 " pending_failed=%" PRIu64 " pending_replaced=%" PRIu64
                    " matched=%" PRIu64 " filtered=%" PRIu64 " eligible=%" PRIu64 " abandoned=%" PRIu64
                    " pending_at_stop=%" PRIu64 " exit_without_entry=%" PRIu64 " pending_delete_failed=%" PRIu64
                    " args_unavailable=%" PRIu64,
                    (uint64_t)total.entered, (uint64_t)total.pending_failed, (uint64_t)total.pending_replaced,
                    (uint64_t)total.matched, (uint64_t)total.filtered, eligible, (uint64_t)total.abandoned,
                    pending[id], (uint64_t)total.exit_without_entry, (uint64_t)total.pending_delete_failed,
                    (uint64_t)total.args_unavailable);
        }
        const char *accounting = !balanced ? "mismatch" : drained ? "ok" : "incomplete";
        fprintf(stderr, " emitted=%" PRIu64 " ring_lost=%" PRIu64 " received=%" PRIu64
                " written=%" PRIu64 " stale=%" PRIu64 " rejected=%" PRIu64 " failed=%" PRIu64 " accounting=%s\n",
                (uint64_t)total.emitted, (uint64_t)total.ring_lost, user->received, user->written,
                user->stale, user->rejected, user->failed, accounting);
        all_balanced = all_balanced && balanced;
    }
    free(values);
    fprintf(stderr, "DIO stats: drain_complete=%s output_ok=%s unknown_received=%" PRIu64 " accounting=%s\n",
            drained ? "true" : "false", output_failed ? "false" : "true", stats->unknown_received,
            !all_balanced ? "mismatch" : drained ? "ok" : "incomplete");
    return all_balanced && drained ? 0 : -1;
}

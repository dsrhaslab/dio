#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "writer.h"
#include "dio.skel.h"

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;

static void stop(int signal_number)
{
    (void)signal_number;
    stopping = 1;
}

static int libbpf_log(enum libbpf_print_level level, const char *format, va_list args)
{
    if (level == LIBBPF_DEBUG) {
        return 0;
    }
    return vfprintf(stderr, format, args);
}

static bool targets_alive(const struct config *cfg)
{
    const uint32_t *ids = cfg->pids;
    unsigned count = cfg->nr_pids;
    bool filter_tid = cfg->nr_tids > 0;

    if (filter_tid) {
        ids = cfg->tids;
        count = cfg->nr_tids;
    }
    for (unsigned i = 0; i < count; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%u/status", ids[i]);
        FILE *status = fopen(path, "r");
        if (status == NULL) {
            if (errno != ENOENT && errno != ESRCH) {
                return true;
            }
            continue;
        }

        char line[256];
        char state = 0;
        unsigned threads = 0;
        while (fgets(line, sizeof(line), status) != NULL) {
            if (sscanf(line, "State: %c", &state) == 1) {
                continue;
            }
            if (sscanf(line, "Threads: %u", &threads) == 1) {
                break;
            }
        }
        fclose(status);
        if (state != 'Z' && state != 'X') {
            return true;
        }
        // a thread principal pode ter terminado e ainda haver outras a correr
        if (!filter_tid && threads > 1) {
            return true;
        }
    }
    return false;
}

static int add_targets(int map_fd, const uint32_t *ids, unsigned count)
{
    unsigned char yes = 1;
    for (unsigned i = 0; i < count; i++) {
        if (bpf_map_update_elem(map_fd, &ids[i], &yes, BPF_ANY) != 0) {
            return -1;
        }
    }
    return 0;
}

static uint64_t monotonic_ns(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * 1000000000ULL + now.tv_nsec;
}

static int pending_counts(int map_fd, uint64_t pending[DIO_EVENT_MAX])
{
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

static void add_stats(struct dio_kernel_stats *total, const struct dio_kernel_stats *cpu)
{
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

static int print_stats(struct dio_bpf *skel, const struct dio_writer *writer, int cpus, bool drained)
{
    static const char *names[DIO_EVENT_MAX] = {
        [DIO_PATH] = "EventPath", [DIO_OPENAT] = "openat", [DIO_READ] = "read",
        [DIO_PREAD64] = "pread64", [DIO_READV] = "readv", [DIO_WRITE] = "write",
        [DIO_PWRITE64] = "pwrite64", [DIO_WRITEV] = "writev", [DIO_CLOSE] = "close",
        [DIO_FSYNC] = "fsync", [DIO_FDATASYNC] = "fdatasync"
    };
    uint64_t pending[DIO_EVENT_MAX] = {0};
    if (pending_counts(bpf_map__fd(skel->maps.entry_syscall_args), pending) != 0 ||
        pending_counts(bpf_map__fd(skel->maps.entry_data_buf_args), pending) != 0 ||
        pending_counts(bpf_map__fd(skel->maps.close_file_tag), pending) != 0) {
        perror("ler chamadas pendentes");
        return -1;
    }

    // o lookup devolve uma copia por CPU, cada uma alinhada a 8 bytes
    size_t stride = (sizeof(struct dio_kernel_stats) + 7) & ~(size_t)7;
    unsigned char *values = calloc((size_t)cpus, stride);
    if (values == NULL) {
        perror("memoria para contadores");
        return -1;
    }
    bool all_balanced = writer->unknown_received == 0;
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
        const struct dio_user_stats *user = &writer->stats[id];
        bool balanced = user->received == user->written + user->stale + user->rejected + user->failed;
        if (drained && total.emitted != user->received) {
            balanced = false;
        }
        fprintf(stderr, "DIO stats: event=%s", names[id]);
        if (id == DIO_PATH) {
            balanced = balanced && total.path_attempted == total.path_prepare_failed + total.emitted + total.ring_lost;
            fprintf(stderr, " attempted=%" PRIu64 " prepare_failed=%" PRIu64,
                    (uint64_t)total.path_attempted, (uint64_t)total.path_prepare_failed);
        } else {
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
            drained ? "true" : "false", writer->output_failed ? "false" : "true", writer->unknown_received,
            !all_balanced ? "mismatch" : drained ? "ok" : "incomplete");
    return all_balanced && drained ? 0 : -1;
}

int main(int argc, char **argv)
{
    struct config cfg;
    int result = config_read(&cfg, argc, argv);
    if (result != 0) {
        return result < 0 ? 2 : 0;
    }
    if (!targets_alive(&cfg)) {
        fprintf(stderr, "Nenhum PID/TID indicado esta ativo\n");
        return 1;
    }

    struct sigaction action = {0};
    action.sa_handler = stop;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) != 0 || sigaction(SIGTERM, &action, NULL) != 0) {
        perror("sigaction");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    libbpf_set_print(libbpf_log);

    struct dio_bpf *skel = NULL;
    struct ring_buffer *ring = NULL;
    struct dio_writer writer = {0};
    bool writer_open = false;
    bool attached = false;
    bool capturing = false;
    bool drained = true;
    int cpus = 0;
    int rc = 1;

    // escolhe os filtros e os programas antes de carregar
    skel = dio_bpf__open();
    if (skel == NULL) {
        fprintf(stderr, "Nao consegui abrir o BPF\n");
        goto cleanup;
    }
    skel->rodata->filter_pids = cfg.nr_pids > 0;
    skel->rodata->filter_tids = cfg.nr_tids > 0;
    skel->rodata->skip_errors = cfg.discard_errors;
    skel->rodata->skip_dirs = cfg.discard_directories;
    skel->rodata->collect_stats = cfg.show_stats;

    struct {
        struct bpf_program *enter;
        struct bpf_program *exit;
        bool enabled;
    } syscalls[] = {
        {skel->progs.enter_openat, skel->progs.exit_openat, cfg.openat},
        {skel->progs.enter_read, skel->progs.exit_read, cfg.read},
        {skel->progs.enter_write, skel->progs.exit_write, cfg.write},
        {skel->progs.enter_close, skel->progs.exit_close, cfg.close},
        {skel->progs.enter_pread64, skel->progs.exit_pread64, cfg.pread64},
        {skel->progs.enter_pwrite64, skel->progs.exit_pwrite64, cfg.pwrite64},
        {skel->progs.enter_readv, skel->progs.exit_readv, cfg.readv},
        {skel->progs.enter_writev, skel->progs.exit_writev, cfg.writev},
        {skel->progs.enter_fsync, skel->progs.exit_fsync, cfg.fsync},
        {skel->progs.enter_fdatasync, skel->progs.exit_fdatasync, cfg.fdatasync}
    };
    for (unsigned i = 0; i < sizeof(syscalls) / sizeof(syscalls[0]); i++) {
        if (bpf_program__set_autoload(syscalls[i].enter, syscalls[i].enabled) != 0 ||
            bpf_program__set_autoload(syscalls[i].exit, syscalls[i].enabled) != 0) {
            fprintf(stderr, "Nao consegui selecionar as syscalls\n");
            goto cleanup;
        }
    }
    if (dio_bpf__load(skel) != 0) {
        fprintf(stderr, "Nao consegui carregar o BPF\n");
        goto cleanup;
    }
    if (add_targets(bpf_map__fd(skel->maps.trace_pids), cfg.pids, cfg.nr_pids) != 0 ||
        add_targets(bpf_map__fd(skel->maps.trace_tids), cfg.tids, cfg.nr_tids) != 0) {
        perror("filtros PID/TID");
        goto cleanup;
    }

    cpus = libbpf_num_possible_cpus();
    if (cpus <= 0) {
        fprintf(stderr, "Nao consegui ler o numero de CPUs\n");
        goto cleanup;
    }
    const char *output = cfg.output[0] == '\0' ? NULL : cfg.output;
    if (dio_writer_open(&writer, output, bpf_map__fd(skel->maps.percpu_array_files), cpus, cfg.session_name) != 0) {
        perror("abrir output");
        goto cleanup;
    }
    writer_open = true;
    writer.show_stats = cfg.show_stats;

    // cada registo que chegar chama o writer na mesma thread
    ring = ring_buffer__new(bpf_map__fd(skel->maps.events), dio_writer_record, &writer, NULL);
    if (ring == NULL) {
        perror("ring buffer");
        goto cleanup;
    }
    if (dio_bpf__attach(skel) != 0) {
        fprintf(stderr, "Nao consegui ligar os hooks\n");
        goto cleanup;
    }
    attached = true;
    uint64_t start = monotonic_ns();
    if (start == 0) {
        perror("relogio");
        goto cleanup;
    }
    skel->bss->accepting = 1;
    capturing = true;
    fprintf(stderr, "DIO ready: %u PIDs, %u TIDs\n", cfg.nr_pids, cfg.nr_tids);

    rc = 0;
    while (!stopping && targets_alive(&cfg)) {
        result = ring_buffer__poll(ring, 100);
        if (result < 0 && result != -EINTR) {
            fprintf(stderr, "ring poll: %s\n", strerror(-result));
            drained = false;
            rc = 1;
            break;
        }
        if (cfg.duration > 0) {
            uint64_t now = monotonic_ns();
            if (now == 0) {
                perror("relogio");
                rc = 1;
                break;
            }
            if (now - start >= (uint64_t)cfg.duration * 1000000000ULL) {
                break;
            }
        }
    }

cleanup:
    // desliga e le o que ja estava no ring
    if (attached) {
        skel->bss->accepting = 0;
        dio_bpf__detach(skel);
    }
    if (ring != NULL) {
        if (writer.output_failed) {
            drained = false;
        } else if (ring_buffer__consume(ring) < 0) {
            drained = false;
            rc = 1;
        }
    }
    if (writer_open && dio_writer_close(&writer) != 0) {
        perror("fechar output");
        rc = 1;
    }
    if (cfg.show_stats && capturing && print_stats(skel, &writer, cpus, drained) != 0) {
        rc = 1;
    }
    ring_buffer__free(ring);
    dio_bpf__destroy(skel);
    return rc;
}

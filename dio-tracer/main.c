#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "writer.h"
#include "dio.skel.h"

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

    if (cfg->nr_tids > 0) {
        ids = cfg->tids;
        count = cfg->nr_tids;
    }
    for (unsigned i = 0; i < count; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%u", ids[i]);
        if (access(path, F_OK) == 0 || errno != ENOENT) {
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

int main(int argc, char **argv)
{
    struct config cfg;
    int result = config_read(&cfg, argc, argv);
    if (result != 0) {
        return result < 0 ? 2 : 0;
    }
    if (!targets_alive(&cfg)) {
        fprintf(stderr, "Nenhum PID/TID indicado existe\n");
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

    struct {
        struct bpf_program *enter;
        struct bpf_program *exit;
        bool enabled;
    } syscalls[] = {
        {skel->progs.enter_openat, skel->progs.exit_openat, cfg.openat},
        {skel->progs.enter_read, skel->progs.exit_read, cfg.read},
        {skel->progs.enter_write, skel->progs.exit_write, cfg.write},
        {skel->progs.enter_close, skel->progs.exit_close, cfg.close}
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

    int cpus = libbpf_num_possible_cpus();
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
    fprintf(stderr, "DIO ready: %u PIDs, %u TIDs\n", cfg.nr_pids, cfg.nr_tids);

    rc = 0;
    while (!stopping && targets_alive(&cfg)) {
        result = ring_buffer__poll(ring, 100);
        if (result < 0 && result != -EINTR) {
            fprintf(stderr, "ring poll: %s\n", strerror(-result));
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
    if (ring != NULL && ring_buffer__consume(ring) < 0) {
        rc = 1;
    }
    if (writer_open && dio_writer_close(&writer) != 0) {
        perror("fechar output");
        rc = 1;
    }
    ring_buffer__free(ring);
    dio_bpf__destroy(skel);
    return rc;
}

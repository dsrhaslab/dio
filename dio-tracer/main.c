#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "consumer.h"
#include "filepaths.h"
#include "loader.h"
#include "stats.h"
#include "writer.h"
#include "dio.skel.h"

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

void stop(int signal_number) {
    (void)signal_number;
    stopping = 1;
}

int libbpf_log(enum libbpf_print_level level, const char *format, va_list args) {
    if (level == LIBBPF_DEBUG) {
        return 0;
    }
    return vfprintf(stderr, format, args);
}

bool targets_alive(const struct config *cfg) {
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

uint64_t monotonic_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * 1000000000ULL + now.tv_nsec;
}

int main(int argc, char **argv) {
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
    struct dio_consumer consumer = {0};
    struct dio_filepaths paths = {0};
    struct dio_stats stats = {.enabled = cfg.show_stats};
    struct dio_writer writer = {0};
    bool writer_open = false;
    bool attached = false;
    bool capturing = false;
    bool drained = true;
    int cpus = 0;
    int rc = 1;

    skel = dio_loader_open(&cfg);
    if (skel == NULL) {
        goto cleanup;
    }

    cpus = libbpf_num_possible_cpus();
    if (cpus <= 0) {
        fprintf(stderr, "Falha ao ler o numero de CPUs\n");
        goto cleanup;
    }

    if (dio_filepaths_open(&paths, bpf_map__fd(skel->maps.path_slots), cpus) != 0) {
        perror("preparar paths");
        goto cleanup;
    }

    const char *output = cfg.output[0] == '\0' ? NULL : cfg.output;
    if (dio_writer_open(&writer, output, cfg.session_name) != 0) {
        perror("abrir output");
        goto cleanup;
    }
    writer_open = true;

    if (dio_consumer_open(&consumer, bpf_map__fd(skel->maps.events), &paths, &writer, &stats) != 0) {
        perror("ring buffer");
        goto cleanup;
    }

    if (dio_loader_attach(skel) != 0) {
        goto cleanup;
    }
    attached = true;
    uint64_t start = monotonic_ns();
    if (start == 0) {
        perror("relogio");
        goto cleanup;
    }
    dio_loader_accept(skel, true);
    capturing = true;
    fprintf(stderr, "DIO ready: %u PIDs, %u TIDs\n", cfg.nr_pids, cfg.nr_tids);

    rc = 0;
    while (!stopping && targets_alive(&cfg)) {
        result = dio_consumer_poll(&consumer, 100);
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
        dio_loader_detach(skel);
    }

    if (consumer.ring != NULL) {
        if (writer.output_failed) {
            drained = false;
        }
        else if (dio_consumer_drain(&consumer) < 0) {
            drained = false;
            rc = 1;
        }
    }

    if (writer_open && dio_writer_close(&writer) != 0) {
        perror("fechar output");
        rc = 1;
    }

    if (cfg.show_stats && capturing && dio_stats_print(skel, &stats, cpus, drained, writer.output_failed) != 0) {
        rc = 1;
    }

    dio_consumer_close(&consumer);
    dio_filepaths_close(&paths);
    dio_loader_destroy(skel);
    return rc;
}

#ifndef DIO_CONFIG_H
#define DIO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_TARGETS 256

struct config {
    uint32_t pids[MAX_TARGETS];
    uint32_t tids[MAX_TARGETS];
    unsigned nr_pids;
    unsigned nr_tids;
    bool openat;
    bool read;
    bool write;
    bool close;
    bool pread64;
    bool pwrite64;
    bool discard_errors;
    bool discard_directories;
    unsigned duration;
    char output[4096];
    char session_name[128];
};

// 0: pronto; 1: ajuda apresentada; -1: configuração inválida.
int config_read(struct config *cfg, int argc, char **argv);

#endif

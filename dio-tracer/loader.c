#include "loader.h"
#include "config.h"
#include "dio.skel.h"

#include <bpf/bpf.h>
#include <stdio.h>

static int add_targets(int map_fd, const uint32_t *ids, unsigned count) {
    unsigned char yes = 1;
    for (unsigned i = 0; i < count; i++) {
        if (bpf_map_update_elem(map_fd, &ids[i], &yes, BPF_ANY) != 0) {
            return -1;
        }
    }
    return 0;
}

struct dio_bpf *dio_loader_open(const struct config *cfg) {
    // escolhe os filtros e os programas antes de carregar
    struct dio_bpf *skel = dio_bpf__open();
    if (skel == NULL) {
        fprintf(stderr, "Falha ao abrir o BPF\n");
        return NULL;
    }
    skel->rodata->filter_pids = cfg->nr_pids > 0;
    skel->rodata->filter_tids = cfg->nr_tids > 0;
    skel->rodata->skip_errors = cfg->discard_errors;
    skel->rodata->skip_dirs = cfg->discard_directories;
    skel->rodata->collect_stats = cfg->show_stats;

    struct {
        struct bpf_program *enter;
        struct bpf_program *exit;
        bool enabled;
    } syscalls[] = {
        {skel->progs.enter_open, skel->progs.exit_open, cfg->open},
        {skel->progs.enter_creat, skel->progs.exit_creat, cfg->creat},
        {skel->progs.enter_openat, skel->progs.exit_openat, cfg->openat},
        {skel->progs.enter_read, skel->progs.exit_read, cfg->read},
        {skel->progs.enter_write, skel->progs.exit_write, cfg->write},
        {skel->progs.enter_close, skel->progs.exit_close, cfg->close},
        {skel->progs.enter_pread64, skel->progs.exit_pread64, cfg->pread64},
        {skel->progs.enter_pwrite64, skel->progs.exit_pwrite64, cfg->pwrite64},
        {skel->progs.enter_readv, skel->progs.exit_readv, cfg->readv},
        {skel->progs.enter_writev, skel->progs.exit_writev, cfg->writev},
        {skel->progs.enter_fsync, skel->progs.exit_fsync, cfg->fsync},
        {skel->progs.enter_fdatasync, skel->progs.exit_fdatasync, cfg->fdatasync},
        {skel->progs.enter_lseek, skel->progs.exit_lseek, cfg->lseek}
    };
    for (unsigned i = 0; i < sizeof(syscalls) / sizeof(syscalls[0]); i++) {
        if (bpf_program__set_autoload(syscalls[i].enter, syscalls[i].enabled) != 0 ||
            bpf_program__set_autoload(syscalls[i].exit, syscalls[i].enabled) != 0) {
            fprintf(stderr, "Falha ao selecionar as syscalls\n");
            goto failed;
        }
    }

    if (dio_bpf__load(skel) != 0) {
        fprintf(stderr, "Falha ao carregar o BPF\n");
        goto failed;
    }

    if (add_targets(bpf_map__fd(skel->maps.trace_pids), cfg->pids, cfg->nr_pids) != 0 ||
        add_targets(bpf_map__fd(skel->maps.trace_tids), cfg->tids, cfg->nr_tids) != 0) {
        perror("filtros PID/TID");
        goto failed;
    }

    return skel;

failed:
    dio_bpf__destroy(skel);
    return NULL;
}

int dio_loader_attach(struct dio_bpf *skel) {
    int result = dio_bpf__attach(skel);
    if (result != 0) {
        fprintf(stderr, "Falha ao ligar os hooks\n");
    }
    return result;
}

void dio_loader_accept(struct dio_bpf *skel, bool accepting) {
    skel->bss->accepting = accepting;
}

void dio_loader_detach(struct dio_bpf *skel) {
    dio_loader_accept(skel, false);
    dio_bpf__detach(skel);
}

void dio_loader_destroy(struct dio_bpf *skel) {
    dio_bpf__destroy(skel);
}

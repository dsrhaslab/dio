#ifndef DIO_LOADER_H
#define DIO_LOADER_H

#include <stdbool.h>

struct config;
struct dio_bpf;

struct dio_bpf *dio_loader_open(const struct config *cfg);
int dio_loader_attach(struct dio_bpf *skel);
void dio_loader_accept(struct dio_bpf *skel, bool accepting);
void dio_loader_detach(struct dio_bpf *skel);
void dio_loader_destroy(struct dio_bpf *skel);

#endif

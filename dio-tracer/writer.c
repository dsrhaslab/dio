#include "writer.h"

#include <bpf/bpf.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int dio_writer_open(struct dio_writer *writer, const char *filename, int files_fd, int possible_cpus)
{
    memset(writer, 0, sizeof(*writer));
    if (files_fd < 0 || possible_cpus <= 0) {
        errno = EINVAL;
        return -1;
    }

    writer->files_fd = files_fd;
    writer->possible_cpus = possible_cpus;
    // o lookup traz um valor por CPU. esta struct tem 1040 bytes, multiplo de 8
    writer->files = calloc(possible_cpus, sizeof(struct dio_file_info));
    if (writer->files == NULL) {
        return -1;
    }

    writer->output = stdout;
    if (filename != NULL) {
        writer->output = fopen(filename, "w");
    }
    if (writer->output == NULL) {
        int error = errno;
        free(writer->files);
        writer->files = NULL;
        errno = error;
        return -1;
    }
    return 0;
}

static void print_path(struct dio_writer *writer, struct dio_path_event *event)
{
    if (event->cpu >= writer->possible_cpus || event->index >= DIO_PATH_SLOTS) {
        return;
    }
    if (bpf_map_lookup_elem(writer->files_fd, &event->index, writer->files) != 0) {
        perror("ler path");
        return;
    }

    struct dio_file_info *info = &writer->files[event->cpu];
    if (event->generation == 0 || info->generation != event->generation) {
        return;
    }
    if (info->offset >= DIO_PATH_LEN || info->size == 0 || info->size > DIO_PATH_LEN - info->offset) {
        return;
    }

    fprintf(writer->output, "PATH dev=%u ino=%u first_ns=%llu path=%.*s\n", event->dev, event->ino, (unsigned long long)event->first_ns, (int)info->size, info->filename + info->offset);
}

static void print_call(struct dio_writer *writer, const char *name, struct dio_base *base, struct dio_fd *file, __u32 flags)
{
    fprintf(writer->output, "pid=%u tid=%u comm=%.*s syscall=%s fd=%d", base->pid, base->tid, DIO_COMM_LEN, base->comm, name, file->fd);
    if (flags & DIO_F_IDENTITY) {
        fprintf(writer->output, " dev=%u ino=%u first_ns=%llu", file->dev, file->ino, (unsigned long long)file->first_ns);
    }
    fprintf(writer->output, " ret=%lld duration_ns=%llu", (long long)base->retval, (unsigned long long)(base->return_ns - base->call_ns));
}

int dio_writer_record(void *context, void *data, size_t size)
{
    struct dio_writer *writer = context;
    struct dio_header header;
    if (data == NULL || size < sizeof(header)) {
        return 0;
    }
    memcpy(&header, data, sizeof(header));

    // ve o tipo e copia para a struct desse evento
    if (header.event_id == DIO_PATH) {
        struct dio_path_event event;
        if (size != sizeof(event)) {
            return 0;
        }
        memcpy(&event, data, sizeof(event));
        print_path(writer, &event);
    } else if (header.event_id == DIO_OPENAT) {
        struct dio_open_event event;
        if (size != sizeof(event)) {
            return 0;
        }
        memcpy(&event, data, sizeof(event));
        print_call(writer, "openat", &event.base, &event.file, header.flags);
        fprintf(writer->output, " flags=%u mode=%u\n", event.flags, (unsigned)event.mode);
    } else if (header.event_id == DIO_READ || header.event_id == DIO_WRITE) {
        struct dio_data_event event;
        if (size != sizeof(event)) {
            return 0;
        }
        memcpy(&event, data, sizeof(event));
        const char *name = "read";
        if (header.event_id == DIO_WRITE) {
            name = "write";
        }
        print_call(writer, name, &event.base, &event.file, header.flags);
        fprintf(writer->output, " requested=%llu", (unsigned long long)event.bytes_requested);
        if (header.flags & DIO_F_OFFSET) {
            fprintf(writer->output, " offset=%lld", (long long)event.offset);
        }
        fprintf(writer->output, "\n");
    } else if (header.event_id == DIO_CLOSE) {
        struct dio_close_event event;
        if (size != sizeof(event)) {
            return 0;
        }
        memcpy(&event, data, sizeof(event));
        print_call(writer, "close", &event.base, &event.file, header.flags);
        fprintf(writer->output, "\n");
    }

    if (fflush(writer->output) != 0 || ferror(writer->output)) {
        return -EIO;
    }
    return 0;
}

int dio_writer_close(struct dio_writer *writer)
{
    int result = 0;
    if (writer->output != NULL) {
        if (fflush(writer->output) != 0 || ferror(writer->output)) {
            result = -1;
        }
        if (writer->output != stdout && fclose(writer->output) != 0) {
            result = -1;
        }
    }
    free(writer->files);
    writer->files = NULL;
    writer->output = NULL;
    return result;
}

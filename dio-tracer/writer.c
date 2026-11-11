#define _GNU_SOURCE
#include "writer.h"

#include <bpf/bpf.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <yyjson.h>

#define PATH_STALE 1

uint64_t time_ns(struct timespec value) {
    // um segundo tem 1000000000 nanossegundos
    return (uint64_t)value.tv_sec * 1000000000ULL + value.tv_nsec;
}

bool format_time(uint64_t ns, char *text, size_t size) {
    time_t seconds = ns / 1000000000ULL;
    struct tm date;
    char whole_seconds[32];

    if (gmtime_r(&seconds, &date) == NULL ||
        strftime(whole_seconds, sizeof(whole_seconds), "%Y-%m-%dT%H:%M:%S", &date) == 0) {
        return false;
    }

    int length = snprintf(text, size, "%s.%09" PRIu64 "Z", whole_seconds, (uint64_t)(ns % 1000000000ULL));
    return length >= 0 && (size_t)length < size;
}

void file_tag(char *text, size_t size, uint32_t dev, uint32_t ino, uint64_t first_ns) {
    snprintf(text, size, "%" PRIu32 "|%" PRIu32 "|%" PRIu64, dev, ino, first_ns);
}

const char *file_type(unsigned mode) {
    switch (mode & S_IFMT) {
        case S_IFSOCK: return "Socket";
        case S_IFLNK: return "Symbolic link";
        case S_IFREG: return "Regular file";
        case S_IFBLK: return "Block device";
        case S_IFDIR: return "Directory";
        case S_IFCHR: return "Char device";
        case S_IFIFO: return "Pipe";
        default: return "Unknown";
    }
}

const char *seek_whence(uint32_t whence, char *text, size_t size) {
    switch (whence) {
        case SEEK_SET: return "SEEK_SET";
        case SEEK_CUR: return "SEEK_CUR";
        case SEEK_END: return "SEEK_END";
        case SEEK_DATA: return "SEEK_DATA";
        case SEEK_HOLE: return "SEEK_HOLE";
        default:
            snprintf(text, size, "%" PRIu32, whence);
            return text;
    }
}

bool open_flags(unsigned value, char *text, size_t size) {
    const struct {
        unsigned value;
        const char *name;
    } flags[] = {
        {O_TMPFILE, "O_TMPFILE"},
        {O_SYNC, "O_SYNC"},
        {O_CREAT, "O_CREAT"},
        {O_EXCL, "O_EXCL"},
        {O_NOCTTY, "O_NOCTTY"},
        {O_TRUNC, "O_TRUNC"},
        {O_APPEND, "O_APPEND"},
        {O_NONBLOCK, "O_NONBLOCK"},
        {O_DSYNC, "O_DSYNC"},
        {O_DIRECT, "O_DIRECT"},
        {O_NOFOLLOW, "O_NOFOLLOW"},
        {O_NOATIME, "O_NOATIME"},
        {O_CLOEXEC, "O_CLOEXEC"},
        {O_PATH, "O_PATH"},
        {O_DIRECTORY, "O_DIRECTORY"},
        {O_ASYNC, "O_ASYNC"}
    };

    const char *access_mode = "";
    switch (value & O_ACCMODE) {
        case O_RDONLY: access_mode = "O_RDONLY"; break;
        case O_WRONLY: access_mode = "O_WRONLY"; break;
        case O_RDWR: access_mode = "O_RDWR"; break;
    }

    int length = snprintf(text, size, "%s", access_mode);
    if (length < 0 || (size_t)length >= size) {
        return false;
    }

    size_t used = length;
    value &= ~O_ACCMODE;

    // O_SYNC e O_TMPFILE incluem outras flags, por isso ficam primeiro na lista.
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
        if ((value & flags[i].value) != flags[i].value) {
            continue;
        }

        length = snprintf(text + used, size - used, "%s%s", used > 0 ? "|" : "", flags[i].name);
        if (length < 0 || (size_t)length >= size - used) {
            return false;
        }

        used += length;
        value &= ~flags[i].value;
    }

    return true;
}

int dio_writer_open(struct dio_writer *writer, const char *filename, int paths_fd, int possible_cpus, const char *session_name) {
    memset(writer, 0, sizeof(*writer));

    if (paths_fd < 0 || possible_cpus <= 0 || session_name == NULL ||
        strlen(session_name) >= sizeof(writer->session_name)) {
        errno = EINVAL;
        return -1;
    }

    struct timespec before, realtime, after;
    if (clock_gettime(CLOCK_MONOTONIC, &before) != 0 ||
        clock_gettime(CLOCK_REALTIME, &realtime) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &after) != 0 ||
        gethostname(writer->hostname, sizeof(writer->hostname)) != 0) {
        return -1;
    }

    // esta diferença passa o tempo do BPF para o tempo de calendário do JSON
    uint64_t monotonic = time_ns(before) + (time_ns(after) - time_ns(before)) / 2;
    writer->epoch_offset_ns = time_ns(realtime) - monotonic;

    if (session_name[0] != '\0') {
        strcpy(writer->session_name, session_name);
    }
    else {
        snprintf(writer->session_name, sizeof(writer->session_name), "dio-%" PRIu64 "-%ld", time_ns(realtime), (long)getpid());
    }

    writer->paths_fd = paths_fd;
    writer->possible_cpus = possible_cpus;
    writer->first = true;

    writer->path_values = calloc(possible_cpus, sizeof(struct dio_path_slot));
    if (writer->path_values == NULL) {
        return -1;
    }

    writer->output = filename == NULL ? stdout : fopen(filename, "w");
    if (writer->output == NULL || fputc('[', writer->output) == EOF || fflush(writer->output) != 0) {
        int error = errno != 0 ? errno : EIO;
        if (writer->output != NULL && writer->output != stdout) {
            fclose(writer->output);
        }

        free(writer->path_values);
        writer->path_values = NULL;
        writer->output = NULL;
        errno = error;
        return -1;
    }

    return 0;
}

int write_document(struct dio_writer *writer, yyjson_mut_doc *doc) {
    size_t size;

    // yyjson troca bytes UTF-8 inválidos pelo carácter de substituição
    yyjson_write_flag flags = YYJSON_WRITE_ESCAPE_UNICODE | YYJSON_WRITE_ALLOW_INVALID_UNICODE;
    char *json = yyjson_mut_write(doc, flags, &size);
    if (json == NULL) {
        return -ENOMEM;
    }

    int result = 0;

    // a vírgula fica antes do próximo objeto, para não sobrar uma no fim
    if ((!writer->first && fputs(",\n", writer->output) == EOF) ||
        fwrite(json, 1, size, writer->output) != size || fflush(writer->output) != 0) {
        writer->output_failed = true;
        result = -EIO;
    }
    else {
        writer->first = false;
    }

    free(json);
    return result;
}

int add_context(struct dio_writer *writer, yyjson_mut_doc *doc, yyjson_mut_val *object,
                       const char *name, const char *category, const char *type, const struct dio_base *base,
                       const struct dio_file_ref *file, unsigned flags) {
    uint64_t called = base->call_ns + writer->epoch_offset_ns;
    uint64_t returned = base->return_ns + writer->epoch_offset_ns;

    // espaco para as datas, tid@hostname e os tres numeros da file_tag
    char time_called[40], time_returned[40], thread[280], tag[64];

    // os erros de syscall do Linux ficam entre -4095 e -1
    if (base->return_ns < base->call_ns || base->retval < -4095 ||
        !format_time(called, time_called, sizeof(time_called)) ||
        !format_time(returned, time_returned, sizeof(time_returned))) {
        return -EINVAL;
    }

    snprintf(thread, sizeof(thread), "%u@%s", base->tid, writer->hostname);

    if (!yyjson_mut_obj_add_strcpy(doc, object, "system_call_name", name) ||
        !yyjson_mut_obj_add_strcpy(doc, object, "time_called", time_called) ||
        !yyjson_mut_obj_add_strcpy(doc, object, "time_returned", time_returned)) {
        return -ENOMEM;
    }

    if (!yyjson_mut_obj_add_uint(doc, object, "call_timestamp", called) ||
        !yyjson_mut_obj_add_uint(doc, object, "return_timestamp", returned) ||
        !yyjson_mut_obj_add_uint(doc, object, "execution_time", base->return_ns - base->call_ns)) {
        return -ENOMEM;
    }

    if (!yyjson_mut_obj_add_strcpy(doc, object, "thread", thread) ||
        !yyjson_mut_obj_add_sint(doc, object, "return_value", base->retval < 0 ? -1 : base->retval)) {
        return -ENOMEM;
    }

    if (!yyjson_mut_obj_add_str(doc, object, "category", category) ||
        !yyjson_mut_obj_add_str(doc, object, "event_type", "storage") ||
        !yyjson_mut_obj_add_strcpy(doc, object, "type", type)) {
        return -ENOMEM;
    }

    if (!yyjson_mut_obj_add_strcpy(doc, object, "hostname", writer->hostname) ||
        !yyjson_mut_obj_add_strncpy(doc, object, "comm", base->comm, strnlen(base->comm, DIO_COMM_LEN)) ||
        !yyjson_mut_obj_add_strcpy(doc, object, "session_name", writer->session_name) ||
        !yyjson_mut_obj_add_uint(doc, object, "cpu", base->cpu)) {
        return -ENOMEM;
    }

    if (base->pid != 0 && !yyjson_mut_obj_add_uint(doc, object, "pid", base->pid)) {
        return -ENOMEM;
    }
    if (base->tid != 0 && !yyjson_mut_obj_add_uint(doc, object, "tid", base->tid)) {
        return -ENOMEM;
    }
    if (base->ppid != 0 && !yyjson_mut_obj_add_uint(doc, object, "ppid", base->ppid)) {
        return -ENOMEM;
    }

    if (base->retval < 0) {
        if (!yyjson_mut_obj_add_strcpy(doc, object, "error_message", strerror((int)-base->retval))) {
            return -ENOMEM;
        }
        return 0;
    }

    if (flags & DIO_F_IDENTITY) {
        file_tag(tag, sizeof(tag), file->dev, file->ino, file->first_ns);
        if (!yyjson_mut_obj_add_strcpy(doc, object, "file_tag", tag)) {
            return -ENOMEM;
        }
    }

    return 0;
}

int write_path(struct dio_writer *writer, const struct dio_path_event *event) {
    if (event->cpu >= writer->possible_cpus || event->index >= DIO_PATH_SLOTS) {
        return -EINVAL;
    }

    if (bpf_map_lookup_elem(writer->paths_fd, &event->index, writer->path_values) != 0) {
        return -EIO;
    }

    struct dio_path_slot *slot = &writer->path_values[event->cpu];
    if (event->generation == 0 || slot->generation != event->generation) {
        // este slot já mudou, o path que lá está pode ser de outro ficheiro
        return PATH_STALE;
    }

    if (slot->offset >= DIO_PATH_LEN || slot->size == 0 || slot->size > DIO_PATH_LEN - slot->offset) {
        return -EINVAL;
    }

    // sobra espaco para acrescentar :[inode] aos pipes e sockets
    char path[DIO_PATH_LEN + 32], tag[64];
    size_t length = strnlen(slot->filename + slot->offset, slot->size);
    memcpy(path, slot->filename + slot->offset, length);
    path[length] = '\0';

    if ((slot->file_type & S_IFMT) == S_IFSOCK ||
        ((slot->file_type & S_IFMT) == S_IFIFO && strcmp(path, "pipe") == 0)) {
        snprintf(path + length, sizeof(path) - length, ":[%u]", event->ino);
    }

    file_tag(tag, sizeof(tag), event->dev, event->ino, event->first_ns);

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) {
        return -ENOMEM;
    }

    yyjson_mut_val *object = yyjson_mut_obj(doc);
    if (object == NULL) {
        yyjson_mut_doc_free(doc);
        return -ENOMEM;
    }

    yyjson_mut_doc_set_root(doc, object);

    bool ok = yyjson_mut_obj_add_str(doc, object, "doc_type", "EventPath") &&
        yyjson_mut_obj_add_strcpy(doc, object, "session_name", writer->session_name) &&
        yyjson_mut_obj_add_strcpy(doc, object, "file_path", path) &&
        yyjson_mut_obj_add_str(doc, object, "file_type", file_type(slot->file_type)) &&
        yyjson_mut_obj_add_strcpy(doc, object, "file_tag", tag);

    int result = -ENOMEM;
    if (ok) {
        result = write_document(writer, doc);
    }

    yyjson_mut_doc_free(doc);
    return result;
}

int write_record(struct dio_writer *writer, const struct dio_header *header, void *data) {
    if (header->event_id == DIO_PATH) {
        struct dio_path_event event;

        memcpy(&event, data, sizeof(event));
        return write_path(writer, &event);
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) {
        return -ENOMEM;
    }

    yyjson_mut_val *object = yyjson_mut_obj(doc);
    yyjson_mut_val *args = yyjson_mut_obj(doc);
    if (object == NULL || args == NULL) {
        yyjson_mut_doc_free(doc);
        return -ENOMEM;
    }

    yyjson_mut_doc_set_root(doc, object);

    bool ok = false;
    int result = -EINVAL;

    if (header->event_id == DIO_OPEN || header->event_id == DIO_CREAT || header->event_id == DIO_OPENAT) {
        struct dio_open_event event;
        char flags[256], mode[16];

        memcpy(&event, data, sizeof(event));

        const char *name;
        switch (header->event_id) {
            case DIO_OPEN: name = "open"; break;
            case DIO_CREAT: name = "creat"; break;
            default: name = "openat"; break;
        }

        result = add_context(writer, doc, object, name, "FileManagement", "metadata", &event.base, &event.file, header->flags);
        ok = result == 0;

        if (header->event_id != DIO_CREAT) {
            ok = ok && open_flags(event.flags, flags, sizeof(flags)) &&
                yyjson_mut_obj_add_strcpy(doc, args, "flags", flags);
        }

        if (event.base.retval >= 0) {
            ok = ok && yyjson_mut_obj_add_sint(doc, args, "file_descriptor", event.file.fd);
        }

        if (header->event_id == DIO_CREAT || (event.flags & O_CREAT) || (event.flags & O_TMPFILE) == O_TMPFILE) {
            snprintf(mode, sizeof(mode), "%#3o", (unsigned)event.mode);
            ok = ok && yyjson_mut_obj_add_strcpy(doc, args, "mode", mode);
        }
    }
    else if (header->event_id == DIO_READ || header->event_id == DIO_WRITE ||
               header->event_id == DIO_PREAD64 || header->event_id == DIO_PWRITE64 ||
               header->event_id == DIO_READV || header->event_id == DIO_WRITEV) {
        struct dio_data_event event;

        memcpy(&event, data, sizeof(event));

        const char *name;
        switch (header->event_id) {
            case DIO_READ: name = "read"; break;
            case DIO_WRITE: name = "write"; break;
            case DIO_PREAD64: name = "pread64"; break;
            case DIO_PWRITE64: name = "pwrite64"; break;
            case DIO_READV: name = "readv"; break;
            default: name = "writev"; break;
        }

        result = add_context(writer, doc, object, name, "FileManagement", "data", &event.base, &event.file, header->flags);
        ok = result == 0 && yyjson_mut_obj_add_sint(doc, args, "file_descriptor", event.file.fd);

        if (header->flags & DIO_F_COUNT) {
            ok = ok && yyjson_mut_obj_add_uint(doc, args, "bytes_requested", event.bytes_requested);
        }

        if ((header->flags & DIO_F_OFFSET) &&
            (event.offset != -1 || header->event_id == DIO_PREAD64 || header->event_id == DIO_PWRITE64)) {
            ok = ok && yyjson_mut_obj_add_sint(doc, args, "offset", event.offset);
        }
    }
    else if (header->event_id == DIO_CLOSE || header->event_id == DIO_FSYNC || header->event_id == DIO_FDATASYNC) {
        struct dio_fd_event event;

        memcpy(&event, data, sizeof(event));

        const char *name = "close";
        const char *category = "FileManagement";
        const char *type = "metadata";
        if (header->event_id != DIO_CLOSE) {
            name = header->event_id == DIO_FSYNC ? "fsync" : "fdatasync";
            category = "DeviceManagement";
            type = "data";
        }

        result = add_context(writer, doc, object, name, category, type, &event.base, &event.file, header->flags);
        ok = result == 0 && yyjson_mut_obj_add_sint(doc, args, "file_descriptor", event.file.fd);
    }
    else if (header->event_id == DIO_LSEEK) {
        struct dio_seek_event event;
        char whence[16];

        memcpy(&event, data, sizeof(event));

        result = add_context(writer, doc, object, "lseek", "FileManagement", "metadata", &event.base, &event.file, header->flags);
        ok = result == 0 && yyjson_mut_obj_add_sint(doc, args, "file_descriptor", event.file.fd) &&
                            yyjson_mut_obj_add_sint(doc, args, "offset", event.offset) &&
                            yyjson_mut_obj_add_strcpy(doc, args, "whence", seek_whence(event.whence, whence, sizeof(whence)));
    }

    if (result == 0) {
        if (!ok || !yyjson_mut_obj_add_val(doc, object, "args", args)) {
            result = -ENOMEM;
        }
        else {
            result = write_document(writer, doc);
        }
    }

    yyjson_mut_doc_free(doc);
    return result;
}

int dio_writer_record(void *context, void *data, size_t size) {
    struct dio_writer *writer = context;
    struct dio_header header;

    if (data == NULL || size < sizeof(header)) {
        if (writer->show_stats) {
            writer->unknown_received++;
        }
        return -EINVAL;
    }

    memcpy(&header, data, sizeof(header));

    size_t expected;
    switch (header.event_id) {
        case DIO_PATH: expected = sizeof(struct dio_path_event); break;
        case DIO_OPEN:
        case DIO_CREAT:
        case DIO_OPENAT: expected = sizeof(struct dio_open_event); break;

        case DIO_READ:
        case DIO_PREAD64:
        case DIO_READV:
        case DIO_WRITE:
        case DIO_PWRITE64:
        case DIO_WRITEV: expected = sizeof(struct dio_data_event); break;

        case DIO_CLOSE:
        case DIO_FSYNC:
        case DIO_FDATASYNC: expected = sizeof(struct dio_fd_event); break;

        case DIO_LSEEK: expected = sizeof(struct dio_seek_event); break;

        default:
            if (writer->show_stats) {
                writer->unknown_received++;
            }
            return -EINVAL;
    }

    struct dio_user_stats *stats = &writer->stats[header.event_id];
    if (writer->show_stats) {
        stats->received++;
    }

    if (size != expected) {
        if (writer->show_stats) {
            stats->rejected++;
        }
        return -EINVAL;
    }

    int result = writer->output_failed ? -EIO : write_record(writer, &header, data);
    if (writer->show_stats) {
        if (result == 0) {
            stats->written++;
        }
        else if (result == PATH_STALE) {
            stats->stale++;
        }
        else if (result == -EINVAL) {
            stats->rejected++;
        }
        else {
            stats->failed++;
        }
    }

    // um path antigo é descartado, mas não precisa de parar a captura
    return result == PATH_STALE ? 0 : result;
}

int dio_writer_close(struct dio_writer *writer) {
    int result = 0;

    if (writer->output != NULL) {
        if (writer->output_failed || fputs("]\n", writer->output) == EOF ||
            fflush(writer->output) != 0 || ferror(writer->output)) {
            writer->output_failed = true;
            result = -1;
        }

        if (writer->output != stdout && fclose(writer->output) != 0) {
            writer->output_failed = true;
            result = -1;
        }

        writer->output = NULL;
    }

    free(writer->path_values);
    writer->path_values = NULL;
    return result;
}

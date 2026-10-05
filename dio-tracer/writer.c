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

static uint64_t time_ns(struct timespec value)
{
    return (uint64_t)value.tv_sec * 1000000000ULL + value.tv_nsec;
}

static bool format_time(uint64_t ns, char *text, size_t size)
{
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

static void file_tag(char *text, size_t size, uint32_t dev, uint32_t ino, uint64_t first_ns)
{
    snprintf(text, size, "%" PRIu32 "|%" PRIu32 "|%" PRIu64, dev, ino, first_ns);
}

static const char *file_type(unsigned mode)
{
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

static bool open_flags(unsigned value, char *text, size_t size)
{
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

int dio_writer_open(struct dio_writer *writer, const char *filename, int files_fd, int possible_cpus, const char *session_name)
{
    memset(writer, 0, sizeof(*writer));
    if (files_fd < 0 || possible_cpus <= 0 || session_name == NULL ||
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
    } else {
        snprintf(writer->session_name, sizeof(writer->session_name), "dio-%" PRIu64 "-%ld", time_ns(realtime), (long)getpid());
    }

    writer->files_fd = files_fd;
    writer->possible_cpus = possible_cpus;
    writer->first = true;
    writer->files = calloc(possible_cpus, sizeof(struct dio_file_info));
    if (writer->files == NULL) {
        return -1;
    }
    writer->output = filename == NULL ? stdout : fopen(filename, "w");
    if (writer->output == NULL || fputc('[', writer->output) == EOF || fflush(writer->output) != 0) {
        int error = errno != 0 ? errno : EIO;
        if (writer->output != NULL && writer->output != stdout) {
            fclose(writer->output);
        }
        free(writer->files);
        writer->files = NULL;
        writer->output = NULL;
        errno = error;
        return -1;
    }
    return 0;
}

static int write_document(struct dio_writer *writer, yyjson_mut_doc *doc)
{
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
        result = -EIO;
    } else {
        writer->first = false;
    }
    free(json);
    return result;
}

static int add_context(struct dio_writer *writer, yyjson_mut_doc *doc, yyjson_mut_val *object,
                       const char *name, const char *type, const struct dio_base *base,
                       const struct dio_fd *file, unsigned flags)
{
    uint64_t called = base->call_ns + writer->epoch_offset_ns;
    uint64_t returned = base->return_ns + writer->epoch_offset_ns;
    char time_called[40], time_returned[40], thread[280], tag[64];

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
    if (!yyjson_mut_obj_add_str(doc, object, "category", "FileManagement") ||
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

static int write_path(struct dio_writer *writer, const struct dio_path_event *event)
{
    if (event->cpu >= writer->possible_cpus || event->index >= DIO_PATH_SLOTS) {
        return -EINVAL;
    }
    if (bpf_map_lookup_elem(writer->files_fd, &event->index, writer->files) != 0) {
        return -errno;
    }
    struct dio_file_info *info = &writer->files[event->cpu];
    if (event->generation == 0 || info->generation != event->generation) {
        // este slot já mudou, o path que lá está pode ser de outro ficheiro
        return 0;
    }
    if (info->offset >= DIO_PATH_LEN || info->size == 0 || info->size > DIO_PATH_LEN - info->offset) {
        return -EINVAL;
    }

    char path[DIO_PATH_LEN + 32], tag[64];
    size_t length = strnlen(info->filename + info->offset, info->size);
    memcpy(path, info->filename + info->offset, length);
    path[length] = '\0';
    if ((info->file_type & S_IFMT) == S_IFSOCK ||
        ((info->file_type & S_IFMT) == S_IFIFO && strcmp(path, "pipe") == 0)) {
        snprintf(path + length, sizeof(path) - length, ":[%u]", event->ino);
    }
    file_tag(tag, sizeof(tag), event->dev, event->ino, event->first_ns);

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *object = yyjson_mut_obj(doc);
    if (doc == NULL || object == NULL) {
        yyjson_mut_doc_free(doc);
        return -ENOMEM;
    }
    yyjson_mut_doc_set_root(doc, object);
    bool ok = yyjson_mut_obj_add_str(doc, object, "doc_type", "EventPath") &&
        yyjson_mut_obj_add_strcpy(doc, object, "session_name", writer->session_name) &&
        yyjson_mut_obj_add_strcpy(doc, object, "file_path", path) &&
        yyjson_mut_obj_add_str(doc, object, "file_type", file_type(info->file_type)) &&
        yyjson_mut_obj_add_strcpy(doc, object, "file_tag", tag);
    int result = -ENOMEM;
    if (ok) {
        result = write_document(writer, doc);
    }
    yyjson_mut_doc_free(doc);
    return result;
}

int dio_writer_record(void *context, void *data, size_t size)
{
    struct dio_writer *writer = context;
    struct dio_header header;
    if (data == NULL || size < sizeof(header)) {
        return -EINVAL;
    }
    memcpy(&header, data, sizeof(header));
    if (header.event_id == DIO_PATH) {
        struct dio_path_event event;
        if (size != sizeof(event)) {
            return -EINVAL;
        }
        memcpy(&event, data, sizeof(event));
        return write_path(writer, &event);
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *object = yyjson_mut_obj(doc);
    yyjson_mut_val *args = yyjson_mut_obj(doc);
    if (doc == NULL || object == NULL || args == NULL) {
        yyjson_mut_doc_free(doc);
        return -ENOMEM;
    }
    yyjson_mut_doc_set_root(doc, object);
    bool ok = false;
    int result = -EINVAL;

    if (header.event_id == DIO_OPENAT && size == sizeof(struct dio_open_event)) {
        struct dio_open_event event;
        char flags[256], mode[16];
        memcpy(&event, data, sizeof(event));
        result = add_context(writer, doc, object, "openat", "metadata", &event.base, &event.file, header.flags);
        ok = result == 0 &&
            open_flags(event.flags, flags, sizeof(flags)) &&
            yyjson_mut_obj_add_strcpy(doc, args, "flags", flags);
        if (event.base.retval >= 0) {
            ok = ok && yyjson_mut_obj_add_sint(doc, args, "file_descriptor", event.file.fd);
        }
        if ((event.flags & O_CREAT) || (event.flags & O_TMPFILE) == O_TMPFILE) {
            snprintf(mode, sizeof(mode), "%#3o", (unsigned)event.mode);
            ok = ok && yyjson_mut_obj_add_strcpy(doc, args, "mode", mode);
        }
    } else if ((header.event_id == DIO_READ || header.event_id == DIO_WRITE ||
                header.event_id == DIO_PREAD64 || header.event_id == DIO_PWRITE64) && size == sizeof(struct dio_data_event)) {
        struct dio_data_event event;
        memcpy(&event, data, sizeof(event));
        const char *name;
        switch (header.event_id) {
        case DIO_READ: name = "read"; break;
        case DIO_WRITE: name = "write"; break;
        case DIO_PREAD64: name = "pread64"; break;
        default: name = "pwrite64"; break;
        }
        result = add_context(writer, doc, object, name, "data", &event.base, &event.file, header.flags);
        ok = result == 0 &&
            yyjson_mut_obj_add_sint(doc, args, "file_descriptor", event.file.fd) &&
            yyjson_mut_obj_add_uint(doc, args, "bytes_requested", event.bytes_requested);
        if ((header.flags & DIO_F_OFFSET) &&
            (event.offset != -1 || header.event_id == DIO_PREAD64 || header.event_id == DIO_PWRITE64)) {
            ok = ok && yyjson_mut_obj_add_sint(doc, args, "offset", event.offset);
        }
    } else if (header.event_id == DIO_CLOSE && size == sizeof(struct dio_close_event)) {
        struct dio_close_event event;
        memcpy(&event, data, sizeof(event));
        result = add_context(writer, doc, object, "close", "metadata", &event.base, &event.file, header.flags);
        ok = result == 0 &&
            yyjson_mut_obj_add_sint(doc, args, "file_descriptor", event.file.fd);
    }

    if (result == 0) {
        if (!ok || !yyjson_mut_obj_add_val(doc, object, "args", args)) {
            result = -ENOMEM;
        } else {
            result = write_document(writer, doc);
        }
    }
    yyjson_mut_doc_free(doc);
    return result;
}

int dio_writer_close(struct dio_writer *writer)
{
    int result = 0;
    if (writer->output != NULL) {
        if (fputs("]\n", writer->output) == EOF || fflush(writer->output) != 0 || ferror(writer->output)) {
            result = -1;
        }
        if (writer->output != stdout && fclose(writer->output) != 0) {
            result = -1;
        }
        writer->output = NULL;
    }
    free(writer->files);
    writer->files = NULL;
    return result;
}

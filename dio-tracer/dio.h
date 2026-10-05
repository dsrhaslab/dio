#ifndef DIO_H
#define DIO_H

#ifndef __VMLINUX_H__
#include <linux/types.h>
#endif

#define DIO_COMM_LEN 16
#define DIO_PATH_LEN 1024
#define DIO_PATH_SLOTS 4096
#define DIO_MAX_PENDING 16384

enum dio_event_id {
    DIO_PATH = 1,
    DIO_OPENAT = 7,
    DIO_READ = 8,
    DIO_PREAD64 = 9,
    DIO_WRITE = 11,
    DIO_PWRITE64 = 12,
    DIO_CLOSE = 14,
};

enum dio_record_flags {
    DIO_F_IDENTITY = 1U << 0,
    DIO_F_FILE_TYPE = 1U << 1,
    DIO_F_OFFSET = 1U << 2,
    DIO_F_PATH_REF = 1U << 3,
};

// structs usadas dos dois lados
struct dio_header {
    __u32 event_id;
    __u32 flags;
};

struct dio_base {
    __u32 tid;
    __u32 pid;
    __u32 ppid;
    __u64 call_ns;
    __u64 return_ns;
    __s64 retval;
    char comm[DIO_COMM_LEN];
    __u16 cpu;
};

// o fd e do processo, a identidade e do ficheiro
struct dio_fd {
    __s32 fd;
    __u32 dev;
    __u32 ino;
    __u64 first_ns;
};

struct dio_open_event {
    struct dio_header header;
    struct dio_base base;
    struct dio_fd file;
    __u32 flags;
    __u16 mode;
};

struct dio_data_event {
    struct dio_header header;
    struct dio_base base;
    struct dio_fd file;
    __u16 file_type;
    __u64 bytes_requested;
    __s64 offset;
};

struct dio_close_event {
    struct dio_header header;
    struct dio_base base;
    struct dio_fd file;
};

// o path em si fica noutro map
struct dio_path_event {
    struct dio_header header;
    __u32 dev;
    __u32 ino;
    __u64 first_ns;
    __u32 index;
    __u32 generation;
    __u16 cpu;
};

struct dio_file_info {
    __u32 generation;
    __u16 file_type;
    __u32 offset;
    __u32 size;
    char filename[DIO_PATH_LEN];
};

struct dio_key {
    __u32 event_id;
    __u32 tid;
};

struct dio_identity_key {
    __u32 dev;
    __u32 ino;
};

struct dio_identity {
    __u64 first_ns;
    // 0 = por enviar, 1 = alguem esta a tratar, 2 = enviado
    __u32 path_state;
};

#endif

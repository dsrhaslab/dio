#ifndef DIO_H
#define DIO_H

#ifndef __VMLINUX_H__
#include <linux/types.h>
#endif

#define DIO_COMM_LEN 16 // limite do comm no kernel, incluindo o '\0'
#define DIO_PATH_LEN 1024 // capacidade do texto em cada slot
#define DIO_PATH_SLOTS 4096 // slots de path por CPU
#define DIO_MAX_PENDING 16384 // limite de entradas em cada map de pendentes
#define DIO_EVENT_MAX 60 // indices ate ao maior ID de syscall, nao o numero de syscalls

// IDs mantidos do DIO original
enum dio_event_id {
    DIO_PATH = 1,
    DIO_CREAT = 5,
    DIO_OPEN = 6,
    DIO_OPENAT = 7,
    DIO_READ = 8,
    DIO_PREAD64 = 9,
    DIO_READV = 10,
    DIO_WRITE = 11,
    DIO_PWRITE64 = 12,
    DIO_WRITEV = 13,
    DIO_CLOSE = 14,
    DIO_FSYNC = 22,
    DIO_FDATASYNC = 23,
    DIO_LSEEK = 59,
};

enum dio_record_flags {
    DIO_F_IDENTITY = 1U << 0,
    DIO_F_FILE_TYPE = 1U << 1,
    DIO_F_OFFSET = 1U << 2,
    DIO_F_PATH_REF = 1U << 3,
    DIO_F_COUNT = 1U << 4,
};

struct dio_kernel_stats {
    __u64 entered;
    __u64 pending_failed;
    __u64 pending_replaced;
    __u64 matched;
    __u64 filtered;
    __u64 emitted;
    __u64 ring_lost;
    __u64 abandoned;
    __u64 exit_without_entry;
    __u64 pending_delete_failed;
    __u64 path_attempted;
    __u64 path_prepare_failed;
    __u64 args_unavailable;
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

// fd po processo, identidade po ficheiro
struct dio_file_ref {
    __s32 fd;
    __u32 dev;
    __u32 ino;
    __u64 first_ns;
};

struct dio_open_event {
    struct dio_header header;
    struct dio_base base;
    struct dio_file_ref file;
    __u32 flags; // opcoes abertura p.e. O_RDONLY != header.flags
    __u16 mode; // perms
};

struct dio_data_event {
    struct dio_header header;
    struct dio_base base;
    struct dio_file_ref file;
    __u16 file_type; // i_modo do inode para o tipo de file
    __u64 bytes_requested;
    __s64 offset;
};

// evento das syscalls que aqui so precisam do fd: close, fsync e fdatasync
struct dio_fd_event {
    struct dio_header header;
    struct dio_base base;
    struct dio_file_ref file;
};

struct dio_seek_event {
    struct dio_header header;
    struct dio_base base;
    struct dio_file_ref file;
    __s64 offset;
    __u32 whence;
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

// texto guardado num slot do map de paths, separado do evento
struct dio_path_slot {
    __u32 generation;
    __u16 file_type;
    __u32 offset; // onde comeca o path no array, nao a posicao do ficheiro
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

// valor da cache por (dev, ino), partilhada entre CPUs
struct dio_identity {
    __u64 first_ns;
    __u32 path_state; // 0 = por enviar, 1 = alguem esta a tratar, 2 = enviado
};

#endif

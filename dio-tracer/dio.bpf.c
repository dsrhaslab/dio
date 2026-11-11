#include "vmlinux.h"
#include <asm/unistd_64.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "dio.h"

#define DIO_CREAT_FLAGS (01 | 0100 | 01000) // O_WRONLY | O_CREAT | O_TRUNC no Linux

char LICENSE[] SEC("license") = "GPL";

// o main configura os filtros e ativa accepting depois do attach
const volatile bool filter_pids;
const volatile bool filter_tids;
const volatile bool skip_errors;
const volatile bool skip_dirs;
const volatile bool collect_stats;
volatile __u32 accepting;

// dados observados ao consultar um fd
struct fd_snapshot {
    struct dio_file_ref file;
    __u16 file_type;
    __u32 flags;
    bool discarded;
    __s64 offset;
};

struct open_args {
    __u64 call_ns;
    __u32 flags;
    __u16 mode;
};

struct data_args {
    __u64 call_ns;
    struct fd_snapshot snapshot;
    __u64 bytes_requested;
};

struct fd_args {
    __u64 call_ns;
    struct fd_snapshot snapshot;
};

struct seek_args {
    __u64 call_ns;
    struct fd_snapshot snapshot;
    __s64 offset;
    __u32 whence;
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 8 * 1024 * 1024); // 8 MiB, partilhados por todas as CPUs
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, DIO_EVENT_MAX);
    __type(key, __u32);
    __type(value, struct dio_kernel_stats);
} counters SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 256); // mesmo limite de alvos da configuracao
    __type(key, __u32);
    __type(value, unsigned char);
} trace_pids SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 256); // mesmo limite de alvos da configuracao
    __type(key, __u32);
    __type(value, unsigned char);
} trace_tids SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, DIO_MAX_PENDING);
    __type(key, struct dio_key);
    __type(value, struct open_args);
} entry_syscall_args SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, DIO_MAX_PENDING);
    __type(key, struct dio_key);
    __type(value, struct data_args);
} entry_data_buf_args SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, DIO_MAX_PENDING);
    __type(key, struct dio_key);
    __type(value, struct fd_args);
} entry_fd_args SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, DIO_MAX_PENDING);
    __type(key, struct dio_key);
    __type(value, struct seek_args);
} entry_seek_args SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 65536); // limite escolhido para a cache de identidades
    __type(key, struct dio_identity_key);
    __type(value, struct dio_identity);
} file_identities SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, DIO_PATH_SLOTS);
    __type(key, __u32);
    __type(value, struct dio_path_slot);
} path_slots SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} path_sequence SEC(".maps");

static __always_inline struct dio_kernel_stats *event_stats(__u32 event_id) {
    if (!collect_stats) {
        return 0;
    }
    return bpf_map_lookup_elem(&counters, &event_id);
}

static __always_inline void save_pending(void *map, struct dio_key *key, void *args) {
    struct dio_kernel_stats *stats = event_stats(key->event_id);
    bool replaced = false;

    if (stats) {
        replaced = bpf_map_lookup_elem(map, key) != 0;
    }
    if (bpf_map_update_elem(map, key, args, BPF_ANY)) {
        if (stats) {
            __sync_fetch_and_add(&stats->pending_failed, 1);
        }
    }
    else if (stats && replaced) {
        __sync_fetch_and_add(&stats->pending_replaced, 1);
    }
}

static __always_inline void abandon_pending(void *map, struct dio_key *key) {
    if (bpf_map_delete_elem(map, key) == 0) {
        struct dio_kernel_stats *stats = event_stats(key->event_id);
        if (stats) {
            __sync_fetch_and_add(&stats->abandoned, 1);
        }
    }
}

static __always_inline bool selected_task(void) {
    __u64 pid_tid = bpf_get_current_pid_tgid();
    __u32 tid = pid_tid;
    __u32 pid = pid_tid >> 32; // TGID nos 32 bits superiores

    if (filter_tids) {
        return bpf_map_lookup_elem(&trace_tids, &tid) != 0;
    }
    if (filter_pids) {
        return bpf_map_lookup_elem(&trace_pids, &pid) != 0;
    }
    return false;
}

static __always_inline void init_header(struct dio_header *header, __u32 event_id, __u32 flags) {
    header->event_id = event_id;
    header->flags = flags;
}

static __always_inline void fill_base(struct dio_base *base, __u64 call_ns, __u64 return_ns, __s64 retval) {
    __u64 pid_tid = bpf_get_current_pid_tgid();
    struct task_struct *task = (void *)bpf_get_current_task();

    base->tid = pid_tid;
    base->pid = pid_tid >> 32;
    base->ppid = BPF_CORE_READ(task, real_parent, tgid);
    base->call_ns = call_ns;
    base->return_ns = return_ns;
    base->retval = retval;
    bpf_get_current_comm(base->comm, sizeof(base->comm));
    base->cpu = bpf_get_smp_processor_id();
}

static __always_inline int emit(void *record, __u32 size) {
    struct dio_header *header = record;
    struct dio_kernel_stats *stats = event_stats(header->event_id);
    int result = bpf_ringbuf_output(&events, record, size, 0);

    if (stats) {
        if (result == 0) {
            __sync_fetch_and_add(&stats->emitted, 1);
        }
        else {
            __sync_fetch_and_add(&stats->ring_lost, 1);
        }
    }
    return result;
}

static __always_inline struct file *file_from_fd(__s32 fd) {
    struct task_struct *task = (void *)bpf_get_current_task();
    struct files_struct *files;
    struct fdtable *table;
    struct file **entries;
    struct file *file = 0;
    __u32 max_fds;

    if (fd < 0) {
        return 0;
    }

    files = BPF_CORE_READ(task, files);
    if (!files) {
        return 0;
    }

    table = BPF_CORE_READ(files, fdt);
    if (!table) {
        return 0;
    }

    max_fds = BPF_CORE_READ(table, max_fds);
    if ((__u32)fd >= max_fds) {
        return 0;
    }

    entries = BPF_CORE_READ(table, fd);
    if (!entries) {
        return 0;
    }

    bpf_probe_read_kernel(&file, sizeof(file), &entries[fd]);
    return file;
}

// monta o path de tras para a frente
static __noinline int read_file_path(struct file *file, struct dio_path_slot *slot) {
    struct dentry *dentry = BPF_CORE_READ(file, f_path.dentry);
    struct vfsmount *vfs_mnt = BPF_CORE_READ(file, f_path.mnt);
    struct mount *mnt;
    // este algoritmo escreve para tras a partir da primeira metade do array
    __u32 offset = DIO_PATH_LEN / 2 - 1;
    int complete = 0;

    if (!dentry || !vfs_mnt) {
        return -1;
    }

    // vai buscar o mount a partir do vfsmount
    mnt = (void *)vfs_mnt - bpf_core_field_offset(struct mount, mnt);
    slot->filename[DIO_PATH_LEN / 2 - 1] = '\0';

    // limite de passos ao subir dentries e montagens
    for (int i = 0; i < 64; i++) {
        struct dentry *parent = BPF_CORE_READ(dentry, d_parent);
        struct dentry *root = BPF_CORE_READ(vfs_mnt, mnt_root);
        struct mount *mnt_parent = BPF_CORE_READ(mnt, mnt_parent);
        struct qstr name = {};
        __u32 length;

        if (!parent || !root || !mnt_parent) {
            return -1;
        }

        if (dentry == root && mnt != mnt_parent) {
            dentry = BPF_CORE_READ(mnt, mnt_mountpoint);
            mnt = mnt_parent;
            vfs_mnt = __builtin_preserve_access_index(&mnt->mnt);
            continue;
        }

        if (dentry == root || dentry == parent) {
            if (offset == DIO_PATH_LEN / 2 - 1) {
                __u16 type = slot->file_type & 0170000; // bits do tipo em i_mode
                if (type == 0010000) { // FIFO
                    __builtin_memcpy(slot->filename, "pipe", 5);
                    slot->offset = 0;
                    slot->size = 4;
                    return 0;
                }
                if (type == 0140000) { // socket
                    __builtin_memcpy(slot->filename, "socket", 7);
                    slot->offset = 0;
                    slot->size = 6;
                    return 0;
                }
                BPF_CORE_READ_INTO(&name, dentry, d_name);
                length = name.len;
                // no maximo 255 bytes por componente neste algoritmo
                if (length > 0 && length < 256 && length < offset) {
                    offset -= length;
                    if (bpf_probe_read_kernel(&slot->filename[offset], length, name.name)) {
                        return -1;
                    }
                }
                else {
                    offset--;
                    slot->filename[offset] = '/';
                }
            }
            complete = 1;
            break;
        }

        BPF_CORE_READ_INTO(&name, dentry, d_name);
        length = name.len;
        if (!length || length > 255 || length + 1 > offset) {
            return -1;
        }

        offset -= length;
        if (offset >= DIO_PATH_LEN / 2) {
            return -1;
        }

        if (bpf_probe_read_kernel(&slot->filename[offset], length, name.name)) {
            return -1;
        }

        offset--;
        if (offset >= DIO_PATH_LEN / 2) {
            return -1;
        }

        slot->filename[offset] = '/';
        dentry = parent;
    }

    if (!complete) {
        return -1;
    }

    slot->offset = offset;
    slot->size = DIO_PATH_LEN / 2 - 1 - offset;
    if (slot->size == 0) {
        return -1;
    }

    return 0;
}

static __noinline int submit_path(struct file *file, struct dio_identity *identity, struct dio_file_ref *file_ref, __u16 file_type) {
    struct dio_path_event event = {};
    struct dio_path_slot *slot;
    struct dio_kernel_stats *stats = event_stats(DIO_PATH);
    __u32 zero = 0;
    __u32 *sequence = bpf_map_lookup_elem(&path_sequence, &zero);
    __u32 generation;

    if (stats) {
        __sync_fetch_and_add(&stats->path_attempted, 1);
    }
    if (!sequence) {
        goto failed;
    }

    generation = __sync_add_and_fetch(sequence, 1);
    // zero esta reservado para um slot ainda em preenchimento
    if (!generation) {
        generation = __sync_add_and_fetch(sequence, 1);
    }

    event.index = generation % DIO_PATH_SLOTS;
    slot = bpf_map_lookup_elem(&path_slots, &event.index);
    if (!slot) {
        goto failed;
    }

    // enquanto esta a preencher fica a zero
    slot->generation = 0;
    slot->file_type = file_type;
    if (read_file_path(file, slot)) {
        goto failed;
    }

    // primeiro o path, so depois a geracao
    asm volatile("" ::: "memory");
    slot->generation = generation;
    event.generation = generation;
    event.dev = file_ref->dev;
    event.ino = file_ref->ino;
    event.first_ns = identity->first_ns;
    event.cpu = bpf_get_smp_processor_id();
    init_header(&event.header, DIO_PATH, DIO_F_IDENTITY | DIO_F_FILE_TYPE | DIO_F_PATH_REF);
    return emit(&event, sizeof(event));

failed:
    stats = event_stats(DIO_PATH);
    if (stats) {
        __sync_fetch_and_add(&stats->path_prepare_failed, 1);
    }
    return -1;
}

static __noinline void snapshot_fd(struct fd_snapshot *snapshot, __u64 call_ns) {
    struct file *file = file_from_fd(snapshot->file.fd);
    struct inode *inode;
    struct super_block *sb;
    struct dio_identity_key key = {};
    struct dio_identity initial = { .first_ns = call_ns };
    struct dio_identity *identity;
    __u64 ino;

    snapshot->offset = -1;
    if (!file) {
        return;
    }

    inode = BPF_CORE_READ(file, f_inode);
    if (!inode) {
        return;
    }

    snapshot->file_type = BPF_CORE_READ(inode, i_mode);
    snapshot->flags |= DIO_F_FILE_TYPE;
    // isola os bits do tipo e compara com diretorio
    if (skip_dirs && (snapshot->file_type & 0170000) == 0040000) {
        snapshot->discarded = 1;
        return;
    }
    snapshot->offset = BPF_CORE_READ(file, f_pos);
    snapshot->flags |= DIO_F_OFFSET;
    sb = BPF_CORE_READ(inode, i_sb);
    if (!sb) {
        return;
    }

    ino = BPF_CORE_READ(inode, i_ino);
    // o contrato atual guarda o inode em 32 bits
    if (ino > 0xffffffffULL) {
        return;
    }
    key.dev = BPF_CORE_READ(sb, s_dev);
    key.ino = ino;

    identity = bpf_map_lookup_elem(&file_identities, &key);
    if (!identity) {
        bpf_map_update_elem(&file_identities, &key, &initial, BPF_NOEXIST);
        // outra CPU pode ter chegado primeiro
        identity = bpf_map_lookup_elem(&file_identities, &key);
        if (!identity) {
            return;
        }
    }

    snapshot->file.dev = key.dev;
    snapshot->file.ino = key.ino;
    snapshot->file.first_ns = identity->first_ns;
    snapshot->flags |= DIO_F_IDENTITY;
    // so uma CPU manda este path
    if (__sync_val_compare_and_swap(&identity->path_state, 0, 1) == 0) {
        if (submit_path(file, identity, &snapshot->file, snapshot->file_type) == 0) {
            __sync_lock_test_and_set(&identity->path_state, 2);
        }
        else {
            __sync_lock_test_and_set(&identity->path_state, 0);
        }
    }
}

static __always_inline int enter_open_common(__u32 event_id, __u32 flags, __u16 mode) {
    struct dio_key key = {
        .event_id = event_id,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct open_args args = {};
    struct dio_kernel_stats *stats;

    if (!accepting || !selected_task()) {
        return 0;
    }

    stats = event_stats(event_id);
    if (stats) {
        __sync_fetch_and_add(&stats->entered, 1);
    }

    args.call_ns = bpf_ktime_get_ns();
    args.flags = flags;
    args.mode = mode;

    save_pending(&entry_syscall_args, &key, &args);

    return 0;
}

static __always_inline int exit_open_common(struct trace_event_raw_sys_exit *ctx, __u32 event_id) {
    __u64 return_ns = bpf_ktime_get_ns();
    const volatile __s64 retval = ctx->ret;
    struct dio_key key = {
        .event_id = event_id,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct open_args *pending = bpf_map_lookup_elem(&entry_syscall_args, &key);
    struct open_args args;
    struct fd_snapshot snapshot = {};
    struct dio_open_event event = {};
    struct dio_kernel_stats *stats;

    if (!pending) {
        if (accepting && selected_task()) {
            stats = event_stats(event_id);
            if (stats) {
                __sync_fetch_and_add(&stats->exit_without_entry, 1);
            }
        }
        return 0;
    }

    stats = event_stats(event_id);
    if (stats) {
        __sync_fetch_and_add(&stats->matched, 1);
    }
    // copia antes de apagar
    args = *pending;
    if (bpf_map_delete_elem(&entry_syscall_args, &key) && stats) {
        __sync_fetch_and_add(&stats->pending_delete_failed, 1);
    }

    snapshot.file.fd = -1;
    if (retval >= 0) {
        snapshot.file.fd = retval;
        snapshot_fd(&snapshot, args.call_ns);
    }

    if (snapshot.discarded || (skip_errors && retval < 0)) {
        if (stats) {
            __sync_fetch_and_add(&stats->filtered, 1);
        }
        return 0;
    }

    init_header(&event.header, event_id, snapshot.flags & DIO_F_IDENTITY);
    fill_base(&event.base, args.call_ns, return_ns, retval);
    event.file = snapshot.file;
    event.flags = args.flags;
    event.mode = args.mode;

    emit(&event, sizeof(event));
    return 0;
}

// estas entradas usam raw por causa da regressao no kernel 7.0.0-38
SEC("tracepoint/raw_syscalls/sys_enter")
int enter_open(struct trace_event_raw_sys_enter *ctx) {
    if (ctx->id != __NR_open) {
        return 0;
    }
    return enter_open_common(DIO_OPEN, ctx->args[1], ctx->args[2]);
}

SEC("tracepoint/syscalls/sys_exit_open")
int exit_open(struct trace_event_raw_sys_exit *ctx) {
    return exit_open_common(ctx, DIO_OPEN);
}

SEC("tracepoint/raw_syscalls/sys_enter")
int enter_creat(struct trace_event_raw_sys_enter *ctx) {
    if (ctx->id != __NR_creat) {
        return 0;
    }
    return enter_open_common(DIO_CREAT, DIO_CREAT_FLAGS, ctx->args[1]);
}

SEC("tracepoint/syscalls/sys_exit_creat")
int exit_creat(struct trace_event_raw_sys_exit *ctx) {
    return exit_open_common(ctx, DIO_CREAT);
}

SEC("tracepoint/raw_syscalls/sys_enter")
int enter_openat(struct trace_event_raw_sys_enter *ctx) {
    if (ctx->id != __NR_openat) {
        return 0;
    }
    return enter_open_common(DIO_OPENAT, ctx->args[2], ctx->args[3]);
}

SEC("tracepoint/syscalls/sys_exit_openat")
int exit_openat(struct trace_event_raw_sys_exit *ctx) {
    return exit_open_common(ctx, DIO_OPENAT);
}

// so le os descritores, nao copia o conteudo dos buffers
static __noinline int iov_requested(const struct iovec *vec, __u64 count, __u64 *total) {
    __u64 sum = 0;

    // limite de iovecs aceite aqui e pelo Linux neste alvo
    if (count > 1024) {
        return -1;
    }
    for (__u32 i = 0; i < 1024; i++) {
        struct iovec iov = {};

        if (i >= count) {
            break;
        }
        if (bpf_probe_read_user(&iov, sizeof(iov), &vec[i])) {
            return -1;
        }
        // a soma tem de caber num inteiro de 64 bits com sinal
        if (iov.iov_len > 0x7fffffffffffffffULL - sum) {
            return -1;
        }
        sum += iov.iov_len;
    }
    *total = sum;
    return 0;
}

static __always_inline int enter_data(struct trace_event_raw_sys_enter *ctx, __u32 event_id, bool explicit_offset) {
    struct dio_key key = {
        .event_id = event_id,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct data_args args = {};
    struct dio_kernel_stats *stats;

    if (!accepting || !selected_task()) {
        return 0;
    }

    stats = event_stats(event_id);
    if (stats) {
        __sync_fetch_and_add(&stats->entered, 1);
    }

    args.call_ns = bpf_ktime_get_ns();
    args.snapshot.file.fd = ctx->args[0];
    if (event_id == DIO_READV || event_id == DIO_WRITEV) {
        if (iov_requested((const struct iovec *)ctx->args[1], ctx->args[2], &args.bytes_requested) == 0) {
            args.snapshot.flags |= DIO_F_COUNT;
        }
        else if (stats) {
            __sync_fetch_and_add(&stats->args_unavailable, 1);
        }
    }
    else {
        args.bytes_requested = ctx->args[2];
        args.snapshot.flags |= DIO_F_COUNT;
    }

    snapshot_fd(&args.snapshot, args.call_ns);
    if (explicit_offset) {
        // pread/pwrite recebem o offset mesmo quando o fd e invalido.
        args.snapshot.offset = (__s64)ctx->args[3];
        args.snapshot.flags |= DIO_F_OFFSET;
    }

    save_pending(&entry_data_buf_args, &key, &args);
    return 0;
}

static __always_inline int exit_data(struct trace_event_raw_sys_exit *ctx, __u32 event_id) {
    __u64 return_ns = bpf_ktime_get_ns();
    const volatile __s64 retval = ctx->ret;
    struct dio_key key = {
        .event_id = event_id,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct data_args *pending = bpf_map_lookup_elem(&entry_data_buf_args, &key);
    struct data_args args;
    struct dio_data_event event = {};
    struct dio_kernel_stats *stats;

    if (!pending) {
        if (accepting && selected_task()) {
            stats = event_stats(event_id);
            if (stats) {
                __sync_fetch_and_add(&stats->exit_without_entry, 1);
            }
        }
        return 0;
    }

    stats = event_stats(event_id);
    if (stats) {
        __sync_fetch_and_add(&stats->matched, 1);
    }
    // copia antes de apagar
    args = *pending;
    if (bpf_map_delete_elem(&entry_data_buf_args, &key) && stats) {
        __sync_fetch_and_add(&stats->pending_delete_failed, 1);
    }

    if (args.snapshot.discarded || (skip_errors && retval < 0)) {
        if (stats) {
            __sync_fetch_and_add(&stats->filtered, 1);
        }
        return 0;
    }

    init_header(&event.header, event_id, args.snapshot.flags);
    fill_base(&event.base, args.call_ns, return_ns, retval);
    event.file = args.snapshot.file;
    event.file_type = args.snapshot.file_type;
    event.bytes_requested = args.bytes_requested;
    event.offset = args.snapshot.offset;

    emit(&event, sizeof(event));
    return 0;
}

SEC("tracepoint/syscalls/sys_enter_read")
int enter_read(struct trace_event_raw_sys_enter *ctx) {
    return enter_data(ctx, DIO_READ, false);
}

SEC("tracepoint/syscalls/sys_exit_read")
int exit_read(struct trace_event_raw_sys_exit *ctx) {
    return exit_data(ctx, DIO_READ);
}

SEC("tracepoint/syscalls/sys_enter_readv")
int enter_readv(struct trace_event_raw_sys_enter *ctx) {
    return enter_data(ctx, DIO_READV, false);
}

SEC("tracepoint/syscalls/sys_exit_readv")
int exit_readv(struct trace_event_raw_sys_exit *ctx) {
    return exit_data(ctx, DIO_READV);
}

SEC("tracepoint/raw_syscalls/sys_enter")
int enter_write(struct trace_event_raw_sys_enter *ctx) {
    if (ctx->id != __NR_write) {
        return 0;
    }
    return enter_data(ctx, DIO_WRITE, false);
}

SEC("tracepoint/syscalls/sys_exit_write")
int exit_write(struct trace_event_raw_sys_exit *ctx) {
    return exit_data(ctx, DIO_WRITE);
}

SEC("tracepoint/syscalls/sys_enter_writev")
int enter_writev(struct trace_event_raw_sys_enter *ctx) {
    return enter_data(ctx, DIO_WRITEV, false);
}

SEC("tracepoint/syscalls/sys_exit_writev")
int exit_writev(struct trace_event_raw_sys_exit *ctx) {
    return exit_data(ctx, DIO_WRITEV);
}

SEC("tracepoint/syscalls/sys_enter_pread64")
int enter_pread64(struct trace_event_raw_sys_enter *ctx) {
    return enter_data(ctx, DIO_PREAD64, true);
}

SEC("tracepoint/syscalls/sys_exit_pread64")
int exit_pread64(struct trace_event_raw_sys_exit *ctx) {
    return exit_data(ctx, DIO_PREAD64);
}

SEC("tracepoint/raw_syscalls/sys_enter")
int enter_pwrite64(struct trace_event_raw_sys_enter *ctx) {
    if (ctx->id != __NR_pwrite64) {
        return 0;
    }
    return enter_data(ctx, DIO_PWRITE64, true);
}

SEC("tracepoint/syscalls/sys_exit_pwrite64")
int exit_pwrite64(struct trace_event_raw_sys_exit *ctx) {
    return exit_data(ctx, DIO_PWRITE64);
}

static __always_inline int enter_fd(struct trace_event_raw_sys_enter *ctx, __u32 event_id) {
    struct dio_key key = {
        .event_id = event_id,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct fd_args args = {};
    struct dio_kernel_stats *stats;

    if (!accepting || !selected_task()) {
        return 0;
    }

    stats = event_stats(event_id);
    if (stats) {
        __sync_fetch_and_add(&stats->entered, 1);
    }

    args.call_ns = bpf_ktime_get_ns();
    args.snapshot.file.fd = ctx->args[0];

    snapshot_fd(&args.snapshot, args.call_ns);

    save_pending(&entry_fd_args, &key, &args);

    return 0;
}

static __always_inline int exit_fd(struct trace_event_raw_sys_exit *ctx, __u32 event_id) {
    __u64 return_ns = bpf_ktime_get_ns();
    const volatile __s64 retval = ctx->ret;
    struct dio_key key = {
        .event_id = event_id,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct fd_args *pending = bpf_map_lookup_elem(&entry_fd_args, &key);
    struct fd_args args;
    struct dio_fd_event event = {};
    struct dio_kernel_stats *stats;

    if (!pending) {
        if (accepting && selected_task()) {
            stats = event_stats(event_id);
            if (stats) {
                __sync_fetch_and_add(&stats->exit_without_entry, 1);
            }
        }
        return 0;
    }

    stats = event_stats(event_id);
    if (stats) {
        __sync_fetch_and_add(&stats->matched, 1);
    }
    // copia antes de apagar
    args = *pending;
    if (bpf_map_delete_elem(&entry_fd_args, &key) && stats) {
        __sync_fetch_and_add(&stats->pending_delete_failed, 1);
    }

    if (args.snapshot.discarded || (skip_errors && retval < 0)) {
        if (stats) {
            __sync_fetch_and_add(&stats->filtered, 1);
        }
        return 0;
    }

    init_header(&event.header, event_id, args.snapshot.flags & DIO_F_IDENTITY);
    fill_base(&event.base, args.call_ns, return_ns, retval);
    event.file = args.snapshot.file;

    emit(&event, sizeof(event));
    return 0;
}

SEC("tracepoint/syscalls/sys_enter_close")
int enter_close(struct trace_event_raw_sys_enter *ctx) {
    return enter_fd(ctx, DIO_CLOSE);
}

SEC("tracepoint/syscalls/sys_exit_close")
int exit_close(struct trace_event_raw_sys_exit *ctx) {
    return exit_fd(ctx, DIO_CLOSE);
}

SEC("tracepoint/syscalls/sys_enter_fsync")
int enter_fsync(struct trace_event_raw_sys_enter *ctx) {
    return enter_fd(ctx, DIO_FSYNC);
}

SEC("tracepoint/syscalls/sys_exit_fsync")
int exit_fsync(struct trace_event_raw_sys_exit *ctx) {
    return exit_fd(ctx, DIO_FSYNC);
}

SEC("tracepoint/syscalls/sys_enter_fdatasync")
int enter_fdatasync(struct trace_event_raw_sys_enter *ctx) {
    return enter_fd(ctx, DIO_FDATASYNC);
}

SEC("tracepoint/syscalls/sys_exit_fdatasync")
int exit_fdatasync(struct trace_event_raw_sys_exit *ctx) {
    return exit_fd(ctx, DIO_FDATASYNC);
}

SEC("tracepoint/syscalls/sys_enter_lseek")
int enter_lseek(struct trace_event_raw_sys_enter *ctx) {
    struct dio_key key = {
        .event_id = DIO_LSEEK,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct seek_args args = {};
    struct dio_kernel_stats *stats;

    if (!accepting || !selected_task()) {
        return 0;
    }

    stats = event_stats(DIO_LSEEK);
    if (stats) {
        __sync_fetch_and_add(&stats->entered, 1);
    }

    args.call_ns = bpf_ktime_get_ns();
    args.snapshot.file.fd = (__s32)ctx->args[0];
    args.offset = (__s64)ctx->args[1];
    args.whence = (__u32)ctx->args[2];

    snapshot_fd(&args.snapshot, args.call_ns);

    save_pending(&entry_seek_args, &key, &args);
    return 0;
}

SEC("tracepoint/syscalls/sys_exit_lseek")
int exit_lseek(struct trace_event_raw_sys_exit *ctx) {
    __u64 return_ns = bpf_ktime_get_ns();
    const volatile __s64 retval = ctx->ret;
    struct dio_key key = {
        .event_id = DIO_LSEEK,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct seek_args *pending = bpf_map_lookup_elem(&entry_seek_args, &key);
    struct seek_args args;
    struct dio_seek_event event = {};
    struct dio_kernel_stats *stats;

    if (!pending) {
        if (accepting && selected_task()) {
            stats = event_stats(DIO_LSEEK);
            if (stats) {
                __sync_fetch_and_add(&stats->exit_without_entry, 1);
            }
        }
        return 0;
    }

    stats = event_stats(DIO_LSEEK);
    if (stats) {
        __sync_fetch_and_add(&stats->matched, 1);
    }

    args = *pending;
    if (bpf_map_delete_elem(&entry_seek_args, &key) && stats) {
        __sync_fetch_and_add(&stats->pending_delete_failed, 1);
    }

    if (args.snapshot.discarded || (skip_errors && retval < 0)) {
        if (stats) {
            __sync_fetch_and_add(&stats->filtered, 1);
        }
        return 0;
    }

    init_header(&event.header, DIO_LSEEK, args.snapshot.flags & DIO_F_IDENTITY);
    fill_base(&event.base, args.call_ns, return_ns, retval);
    event.file = args.snapshot.file;
    event.offset = args.offset;
    event.whence = args.whence;

    emit(&event, sizeof(event));
    return 0;
}

SEC("tracepoint/sched/sched_process_exit")
int thread_exit(void *ctx) {
    struct dio_key key = { .tid = (__u32)bpf_get_current_pid_tgid() };

    key.event_id = DIO_OPEN;
    abandon_pending(&entry_syscall_args, &key);

    key.event_id = DIO_CREAT;
    abandon_pending(&entry_syscall_args, &key);

    key.event_id = DIO_OPENAT;
    abandon_pending(&entry_syscall_args, &key);

    key.event_id = DIO_READ;
    abandon_pending(&entry_data_buf_args, &key);

    key.event_id = DIO_PREAD64;
    abandon_pending(&entry_data_buf_args, &key);

    key.event_id = DIO_READV;
    abandon_pending(&entry_data_buf_args, &key);

    key.event_id = DIO_WRITE;
    abandon_pending(&entry_data_buf_args, &key);

    key.event_id = DIO_PWRITE64;
    abandon_pending(&entry_data_buf_args, &key);

    key.event_id = DIO_WRITEV;
    abandon_pending(&entry_data_buf_args, &key);

    key.event_id = DIO_CLOSE;
    abandon_pending(&entry_fd_args, &key);

    key.event_id = DIO_FSYNC;
    abandon_pending(&entry_fd_args, &key);

    key.event_id = DIO_FDATASYNC;
    abandon_pending(&entry_fd_args, &key);

    key.event_id = DIO_LSEEK;
    abandon_pending(&entry_seek_args, &key);
    return 0;
}

SEC("kprobe/destroy_inode")
int BPF_KPROBE(inode_destroy, struct inode *inode) {
    struct super_block *sb = BPF_CORE_READ(inode, i_sb);
    struct dio_identity_key key = {};
    __u64 ino = BPF_CORE_READ(inode, i_ino);

    // mesmo limite de 32 bits usado ao inserir a identidade
    if (!sb || ino > 0xffffffffULL) {
        return 0;
    }

    key.dev = BPF_CORE_READ(sb, s_dev);
    key.ino = ino;
    bpf_map_delete_elem(&file_identities, &key);
    return 0;
}

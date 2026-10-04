#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "dio.h"

char LICENSE[] SEC("license") = "GPL";

// o main configura os filtros e ativa accepting depois do attach
const volatile bool filter_pids;
const volatile bool filter_tids;
const volatile bool skip_errors;
const volatile bool skip_dirs;
volatile __u32 accepting;

struct fd_snapshot {
    struct dio_fd file;
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

struct close_args {
    __u64 call_ns;
    struct fd_snapshot snapshot;
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 8 * 1024 * 1024);
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 256);
    __type(key, __u32);
    __type(value, unsigned char);
} trace_pids SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 256);
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
    __type(value, struct close_args);
} close_file_tag SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 65536);
    __type(key, struct dio_identity_key);
    __type(value, struct dio_identity);
} opened_fds SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, DIO_PATH_SLOTS);
    __type(key, __u32);
    __type(value, struct dio_file_info);
} percpu_array_files SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} path_sequence SEC(".maps");

static __always_inline int selected_task(void)
{
    __u64 pid_tid = bpf_get_current_pid_tgid();
    __u32 tid = pid_tid;
    __u32 pid = pid_tid >> 32;

    // quando há TIDs, a lista de PIDs não é usada
    if (filter_tids) {
        return bpf_map_lookup_elem(&trace_tids, &tid) != 0;
    }
    if (filter_pids) {
        return bpf_map_lookup_elem(&trace_pids, &pid) != 0;
    }
    return 0;
}

static __always_inline void init_header(struct dio_header *header, __u32 event_id, __u32 flags)
{
    header->event_id = event_id;
    header->flags = flags;
}

static __always_inline void fill_base(struct dio_base *base, __u64 call_ns, __u64 return_ns, __s64 retval)
{
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

static __always_inline int emit(void *record, __u32 size)
{
    return bpf_ringbuf_output(&events, record, size, 0) == 0;
}

static __always_inline struct file *file_from_fd(__s32 fd)
{
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
static __noinline int read_file_path(struct file *file, struct dio_file_info *info)
{
    struct dentry *dentry = BPF_CORE_READ(file, f_path.dentry);
    struct vfsmount *vfs_mnt = BPF_CORE_READ(file, f_path.mnt);
    struct mount *mnt;
    __u32 offset = DIO_PATH_LEN / 2 - 1;
    int complete = 0;

    if (!dentry || !vfs_mnt) {
        return -1;
    }

    // vai buscar o mount a partir do vfsmount
    mnt = (void *)vfs_mnt - bpf_core_field_offset(struct mount, mnt);
    info->filename[DIO_PATH_LEN / 2 - 1] = '\0';

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
                __u16 type = info->file_type & 0170000;
                if (type == 0010000) {
                    __builtin_memcpy(info->filename, "pipe", 5);
                    info->offset = 0;
                    info->size = 4;
                    return 0;
                }
                if (type == 0140000) {
                    __builtin_memcpy(info->filename, "socket", 7);
                    info->offset = 0;
                    info->size = 6;
                    return 0;
                }
                BPF_CORE_READ_INTO(&name, dentry, d_name);
                length = name.len;
                if (length > 0 && length < 256 && length < offset) {
                    offset -= length;
                    if (bpf_probe_read_kernel(&info->filename[offset], length, name.name)) {
                        return -1;
                    }
                } else {
                    offset--;
                    info->filename[offset] = '/';
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

        if (bpf_probe_read_kernel(&info->filename[offset], length, name.name)) {
            return -1;
        }

        offset--;
        if (offset >= DIO_PATH_LEN / 2) {
            return -1;
        }

        info->filename[offset] = '/';
        dentry = parent;
    }

    if (!complete) {
        return -1;
    }

    info->offset = offset;
    info->size = DIO_PATH_LEN / 2 - 1 - offset;
    if (info->size == 0) {
        return -1;
    }

    return 0;
}

static __noinline int submit_path(struct file *file, struct dio_identity *identity, struct dio_fd *fd, __u16 file_type)
{
    struct dio_path_event event = {};
    struct dio_file_info *info;
    __u32 zero = 0;
    __u32 *sequence = bpf_map_lookup_elem(&path_sequence, &zero);
    __u32 generation;

    if (!sequence) {
        return 0;
    }

    generation = __sync_add_and_fetch(sequence, 1);
    if (!generation) {
        generation = __sync_add_and_fetch(sequence, 1);
    }

    event.index = generation % DIO_PATH_SLOTS;
    info = bpf_map_lookup_elem(&percpu_array_files, &event.index);
    if (!info) {
        return 0;
    }

    // enquanto esta a preencher fica a zero
    info->generation = 0;
    info->file_type = file_type;
    if (read_file_path(file, info)) {
        return 0;
    }

    // primeiro o path, so depois a geracao
    asm volatile("" ::: "memory");
    info->generation = generation;
    event.generation = generation;
    event.dev = fd->dev;
    event.ino = fd->ino;
    event.first_ns = identity->first_ns;
    event.cpu = bpf_get_smp_processor_id();
    init_header(&event.header, DIO_PATH, DIO_F_IDENTITY | DIO_F_FILE_TYPE | DIO_F_PATH_REF);
    return emit(&event, sizeof(event));
}

static __noinline void snapshot_fd(struct fd_snapshot *snapshot, __u64 call_ns)
{
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
    if (ino > 0xffffffffULL) {
        return;
    }
    key.dev = BPF_CORE_READ(sb, s_dev);
    key.ino = ino;

    identity = bpf_map_lookup_elem(&opened_fds, &key);
    if (!identity) {
        bpf_map_update_elem(&opened_fds, &key, &initial, BPF_NOEXIST);
        // outra CPU pode ter chegado primeiro
        identity = bpf_map_lookup_elem(&opened_fds, &key);
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
        if (submit_path(file, identity, &snapshot->file, snapshot->file_type)) {
            __sync_lock_test_and_set(&identity->path_state, 2);
        } else {
            __sync_lock_test_and_set(&identity->path_state, 0);
        }
    }
}

SEC("tracepoint/syscalls/sys_enter_openat")
int enter_openat(struct trace_event_raw_sys_enter *ctx)
{
    struct dio_key key = {
        .event_id = DIO_OPENAT,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct open_args args = {};

    if (!accepting || !selected_task()) {
        return 0;
    }

    args.call_ns = bpf_ktime_get_ns();
    args.flags = ctx->args[2];
    args.mode = ctx->args[3];

    bpf_map_update_elem(&entry_syscall_args, &key, &args, BPF_ANY);

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_openat")
int exit_openat(struct trace_event_raw_sys_exit *ctx)
{
    __u64 return_ns = bpf_ktime_get_ns();
    const volatile __s64 retval = ctx->ret;
    struct dio_key key = {
        .event_id = DIO_OPENAT,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct open_args *pending = bpf_map_lookup_elem(&entry_syscall_args, &key);
    struct open_args args;
    struct fd_snapshot snapshot = {};
    struct dio_open_event event = {};

    if (!pending) {
        return 0;
    }

    // copia antes de apagar
    args = *pending;
    bpf_map_delete_elem(&entry_syscall_args, &key);

    snapshot.file.fd = -1;
    if (retval >= 0) {
        snapshot.file.fd = retval;
        snapshot_fd(&snapshot, args.call_ns);
    }

    if (snapshot.discarded || (skip_errors && retval < 0)) {
        return 0;
    }

    init_header(&event.header, DIO_OPENAT, snapshot.flags & DIO_F_IDENTITY);
    fill_base(&event.base, args.call_ns, return_ns, retval);
    event.file = snapshot.file;
    event.flags = args.flags;
    event.mode = args.mode;

    emit(&event, sizeof(event));
    return 0;
}

SEC("tracepoint/syscalls/sys_enter_read")
int enter_read(struct trace_event_raw_sys_enter *ctx)
{
    struct dio_key key = {
        .event_id = DIO_READ,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct data_args args = {};

    if (!accepting || !selected_task()) {
        return 0;
    }

    args.call_ns = bpf_ktime_get_ns();

    args.snapshot.file.fd = ctx->args[0];
    args.bytes_requested = ctx->args[2];

    snapshot_fd(&args.snapshot, args.call_ns);

    bpf_map_update_elem(&entry_data_buf_args, &key, &args, BPF_ANY);

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_read")
int exit_read(struct trace_event_raw_sys_exit *ctx)
{
    __u64 return_ns = bpf_ktime_get_ns();
    const volatile __s64 retval = ctx->ret;
    struct dio_key key = {
        .event_id = DIO_READ,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct data_args *pending = bpf_map_lookup_elem(&entry_data_buf_args, &key);
    struct data_args args;
    struct dio_data_event event = {};

    if (!pending) {
        return 0;
    }

    // copia antes de apagar
    args = *pending;
    bpf_map_delete_elem(&entry_data_buf_args, &key);

    if (args.snapshot.discarded || (skip_errors && retval < 0)) {
        return 0;
    }

    init_header(&event.header, DIO_READ, args.snapshot.flags);
    fill_base(&event.base, args.call_ns, return_ns, retval);
    event.file = args.snapshot.file;
    event.file_type = args.snapshot.file_type;
    event.bytes_requested = args.bytes_requested;
    event.offset = args.snapshot.offset;

    emit(&event, sizeof(event));
    return 0;
}

SEC("tracepoint/syscalls/sys_enter_write")
int enter_write(struct trace_event_raw_sys_enter *ctx)
{
    struct dio_key key = {
        .event_id = DIO_WRITE,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct data_args args = {};

    if (!accepting || !selected_task()) {
        return 0;
    }

    args.call_ns = bpf_ktime_get_ns();

    args.snapshot.file.fd = ctx->args[0];
    args.bytes_requested = ctx->args[2];

    snapshot_fd(&args.snapshot, args.call_ns);

    bpf_map_update_elem(&entry_data_buf_args, &key, &args, BPF_ANY);

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_write")
int exit_write(struct trace_event_raw_sys_exit *ctx)
{
    __u64 return_ns = bpf_ktime_get_ns();
    const volatile __s64 retval = ctx->ret;
    struct dio_key key = {
        .event_id = DIO_WRITE,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct data_args *pending = bpf_map_lookup_elem(&entry_data_buf_args, &key);
    struct data_args args;
    struct dio_data_event event = {};

    if (!pending) {
        return 0;
    }

    // copia antes de apagar
    args = *pending;
    bpf_map_delete_elem(&entry_data_buf_args, &key);

    if (args.snapshot.discarded || (skip_errors && retval < 0)) {
        return 0;
    }

    init_header(&event.header, DIO_WRITE, args.snapshot.flags);
    fill_base(&event.base, args.call_ns, return_ns, retval);
    event.file = args.snapshot.file;
    event.file_type = args.snapshot.file_type;
    event.bytes_requested = args.bytes_requested;
    event.offset = args.snapshot.offset;

    emit(&event, sizeof(event));
    return 0;
}

SEC("tracepoint/syscalls/sys_enter_close")
int enter_close(struct trace_event_raw_sys_enter *ctx)
{
    struct dio_key key = {
        .event_id = DIO_CLOSE,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct close_args args = {};

    if (!accepting || !selected_task()) {
        return 0;
    }

    args.call_ns = bpf_ktime_get_ns();
    args.snapshot.file.fd = ctx->args[0];

    snapshot_fd(&args.snapshot, args.call_ns);

    bpf_map_update_elem(&close_file_tag, &key, &args, BPF_ANY);

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_close")
int exit_close(struct trace_event_raw_sys_exit *ctx)
{
    __u64 return_ns = bpf_ktime_get_ns();
    const volatile __s64 retval = ctx->ret;
    struct dio_key key = {
        .event_id = DIO_CLOSE,
        .tid = (__u32)bpf_get_current_pid_tgid()
    };
    struct close_args *pending = bpf_map_lookup_elem(&close_file_tag, &key);
    struct close_args args;
    struct dio_close_event event = {};

    if (!pending) {
        return 0;
    }

    // copia antes de apagar
    args = *pending;
    bpf_map_delete_elem(&close_file_tag, &key);

    if (args.snapshot.discarded || (skip_errors && retval < 0)) {
        return 0;
    }

    init_header(&event.header, DIO_CLOSE, args.snapshot.flags & DIO_F_IDENTITY);
    fill_base(&event.base, args.call_ns, return_ns, retval);
    event.file = args.snapshot.file;

    emit(&event, sizeof(event));
    return 0;
}

SEC("tracepoint/sched/sched_process_exit")
int thread_exit(void *ctx)
{
    struct dio_key key = { .tid = (__u32)bpf_get_current_pid_tgid() };

    key.event_id = DIO_OPENAT;
    bpf_map_delete_elem(&entry_syscall_args, &key);

    key.event_id = DIO_READ;
    bpf_map_delete_elem(&entry_data_buf_args, &key);

    key.event_id = DIO_WRITE;
    bpf_map_delete_elem(&entry_data_buf_args, &key);

    key.event_id = DIO_CLOSE;
    bpf_map_delete_elem(&close_file_tag, &key);
    return 0;
}

SEC("kprobe/destroy_inode")
int BPF_KPROBE(inode_destroy, struct inode *inode)
{
    struct super_block *sb = BPF_CORE_READ(inode, i_sb);
    struct dio_identity_key key = {};
    __u64 ino = BPF_CORE_READ(inode, i_ino);

    if (!sb || ino > 0xffffffffULL) {
        return 0;
    }

    key.dev = BPF_CORE_READ(sb, s_dev);
    key.ino = ino;
    bpf_map_delete_elem(&opened_fds, &key);
    return 0;
}

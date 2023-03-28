// +build ignore
// exclude this C file from compilation by the CGO compiler

#include <uapi/linux/ptrace.h>
#include <net/sock.h>
#include <linux/if.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/socket.h>
#include <linux/types.h>
#include <linux/pid_namespace.h>
#include <linux/fdtable.h>
#include <linux/stat.h>
#include <linux/mount.h>
#include <linux/fs.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/un.h>
#include <linux/cdev.h>
#include <linux/blkdev.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mount.h>
//HEADER_CONTENT//

struct pid_info_t {
	uint32_t ppid;
	uint32_t kpid;
	uint32_t tgid;
	char comm[TASK_COMM_LEN];
};

struct event_key_t {
    enum event_type type;
    u32 pid;
};

struct entry_args_t {
    u64 timestamp;
    u64 args[10];
};

// --------------------

BPF_HASH(trace_pids, u32, char);
BPF_HASH(trace_tids, u32, char);

BPF_HASH(entry_syscall_args, struct event_key_t, struct entry_args_t);

BPF_ARRAY(entries_stats, u32, TOTAL_EVENTS);
BPF_ARRAY(exits_stats, u32, TOTAL_EVENTS);
BPF_ARRAY(errors_stats, u32, TOTAL_EVENTS);
BPF_ARRAY(losts_stats, u32, TOTAL_EVENTS);
BPF_ARRAY(discarded_stats, u32, TOTAL_EVENTS);
BPF_ARRAY(index_counter, u64, 2);

#ifdef PROFILE_ON
BPF_HASH(times_logs_call, u64, u64);
BPF_HASH(times_logs_subm, u64, u64);
BPF_HASH(times_logs_lost, u64, u64);
#endif

BPF_PERF_OUTPUT(events);

// --------------------
// Helper functions.

int static comm_equals(const char str1[TASK_COMM_LEN], const char str2[TASK_COMM_LEN])
{
    int size = strlen(str1) & (TASK_COMM_LEN-1);
    for (int i = 0; i < size; i++) {
        if (str1[i] != str2[i]) return 0;
    }
    return 1;
}

int static comm_tracer_comm(const char str1[TASK_COMM_LEN])
{
    for (int i = 0; i < 3; i++) {
        if (str1[i] != TRACER_COMM[i]) {
            return 0;
        }
    }
    return 1;
}

struct pid_info_t static pid_info()
{
	struct pid_info_t process_info = {};
	bpf_get_current_comm(&process_info.comm, sizeof(process_info.comm));

    struct task_struct *task = (struct task_struct *)bpf_get_current_task();
    process_info.ppid = task->real_parent->tgid;
    process_info.kpid = bpf_get_current_pid_tgid();
    process_info.tgid = bpf_get_current_pid_tgid() >> 32;

	return process_info;
}

int static skip_comm_struct(struct pid_info_t process_info)
{
    return (comm_equals(TARGET_COMM, process_info.comm) == 0);
}
int static skip_pid(u32 pid, u32 ppid)
{
    if ( PID_FILTER == 0 || trace_pids.lookup(&pid) != NULL || ( CHILDS_FILTER==1 && trace_pids.lookup(&ppid) != NULL ) )
    {
        return 0;
    }

    return 1;
}
int static skip_tid(u32 tid)
{
    if ( TID_FILTER == 0 || trace_tids.lookup(&tid) != NULL )
    {
        return 0;
    }

    return 1;
}
int static skip(struct pid_info_t process_info)
{
    if (comm_tracer_comm(process_info.comm)) return 1;

	if (PID_FILTER != 0 && COMM_FILTER != 0)
	{
        return (skip_pid(process_info.tgid, process_info.ppid) || skip_comm_struct(process_info));
	}

    if (TID_FILTER != 0)
	{
		return skip_tid(process_info.kpid);
	}

    if (PID_FILTER != 0)
	{
		return skip_pid(process_info.tgid, process_info.ppid);
	}

	if (COMM_FILTER != 0)
	{
		return skip_comm_struct(process_info);
	}

	return 0;
}

// --------------------


void static incrementEnterCounter(enum event_type e_type)
{
    entries_stats.atomic_increment(e_type);
}
void static incrementExitCounter(enum event_type e_type)
{
    exits_stats.atomic_increment(e_type);
}
void static incrementErrorCounter(enum event_type e_type)
{
    errors_stats.atomic_increment(e_type);
}
void static incrementLostCounter(enum event_type e_type)
{
    losts_stats.atomic_increment(e_type);
}
void static incrementDiscardedCounter(enum event_type e_type)
{
    discarded_stats.atomic_increment(e_type);
}

void static handle_calls_event(enum event_type e_type, u64 timestamp, u64 size) {
    #ifdef PROFILE_ON
    u64 sec_timestamp = timestamp / 1000000000;
    u64 *time = times_logs_call.lookup(&sec_timestamp);
    if (time) {
        *time += size;
    } else {
        times_logs_call.insert(&sec_timestamp, &size);
    }
    #endif
}
void static handle_submitted_event(enum event_type e_type, u64 timestamp, u64 size) {
    incrementExitCounter(e_type);
    #ifdef PROFILE_ON
    u64 sec_timestamp = timestamp / 1000000000;
    u64 *time = times_logs_subm.lookup(&sec_timestamp);
    if (time) {
        *time += size;
    } else {
        times_logs_subm.insert(&sec_timestamp, &size);
    }
    #endif
}
void static handle_lost_event(enum event_type e_type, u64 timestamp, u64 size) {
    incrementLostCounter(e_type);
    #ifdef PROFILE_ON
    u64 sec_timestamp = timestamp / 1000000000;
    u64 *time = times_logs_lost.lookup(&sec_timestamp);
    if (time) {
        *time += size;
    } else {
        times_logs_lost.insert(&sec_timestamp, &size);
    }
    #endif
}

static inline int createEventBase(Event *context, enum event_type e_type, struct pid_info_t *proc_info, u64 *call_timestamp, long *return_value)
{
    if (!context) return 1;
    context->etype = e_type;
    context->kpid = proc_info->kpid;
    context->tgid = proc_info->tgid;
    context->ppid = proc_info->ppid;
    context->call_time = *call_timestamp;
    context->return_time = bpf_ktime_get_ns();
    context->return_value = *return_value;
    context->cpu = bpf_get_smp_processor_id();
    bpf_probe_read(&context->comm, sizeof(context->comm), &proc_info->comm);
    return 0;
}

// ----------------------------------

static inline int get_etype(enum event_type *etype, int syscall_nr)
{
    switch (syscall_nr)
    {
    case __NR_read:
        *etype = DIO_READ;
        break;
    case __NR_pread64:
        *etype = DIO_PREAD64;
        break;
    case __NR_readv:
        *etype = DIO_READV;
        break;
    case __NR_write:
        *etype = DIO_WRITE;
        break;
    case __NR_pwrite64:
        *etype = DIO_PWRITE64;
        break;
    case __NR_writev:
        *etype = DIO_WRITEV;
        break;
    case __NR_creat:
        *etype = DIO_CREAT;
        break;
    case __NR_open:
        *etype = DIO_OPEN;
        break;
    case __NR_openat:
        *etype = DIO_OPENAT;
        break;
    case __NR_close:
        *etype = DIO_CLOSE;
        break;
    case __NR_truncate:
        *etype = DIO_TRUNCATE;
        break;
    case __NR_ftruncate:
        *etype = DIO_FTRUNCATE;
        break;
    case __NR_rename:
        *etype = DIO_RENAME;
        break;
    case __NR_renameat:
        *etype = DIO_RENAMEAT;
        break;
    case __NR_renameat2:
        *etype = DIO_RENAMEAT2;
        break;
    case __NR_unlink:
        *etype = DIO_UNLINK;
        break;
    case __NR_unlinkat:
        *etype = DIO_UNLINKAT;
        break;
    case __NR_fsync:
        *etype = DIO_FSYNC;
        break;
    case __NR_fdatasync:
        *etype = DIO_FDATASYNC;
        break;
    case __NR_readahead:
        *etype = DIO_READAHEAD;
        break;
    case __NR_readlink:
        *etype = DIO_READLINK;
        break;
    case __NR_readlinkat:
        *etype = DIO_READLINKAT;
        break;
    case __NR_stat:
        *etype = DIO_STAT;
        break;
    case __NR_lstat:
        *etype = DIO_LSTAT;
        break;
    case __NR_fstat:
        *etype = DIO_FSTAT;
        break;
    case __NR_newfstatat:
        *etype = DIO_FSTATAT;
        break;
    case __NR_fstatfs:
        *etype = DIO_FSTATFS;
        break;
    case __NR_getxattr:
        *etype = DIO_GETXATTR;
        break;
    case __NR_lgetxattr:
        *etype = DIO_LGETXATTR;
        break;
    case __NR_fgetxattr:
        *etype = DIO_FGETXATTR;
        break;
    case __NR_setxattr:
        *etype = DIO_SETXATTR;
        break;
    case __NR_lsetxattr:
        *etype = DIO_LSETXATTR;
        break;
    case __NR_fsetxattr:
        *etype = DIO_FSETXATTR;
        break;
    case __NR_removexattr:
        *etype = DIO_REMOVEXATTR;
        break;
    case __NR_lremovexattr:
        *etype = DIO_LREMOVEXATTR;
        break;
    case __NR_fremovexattr:
        *etype = DIO_FREMOVEXATTR;
        break;
    case __NR_listxattr:
        *etype = DIO_LISTXATTR;
        break;
    case __NR_llistxattr:
        *etype = DIO_LLISTXATTR;
        break;
    case __NR_flistxattr:
        *etype = DIO_FLISTXATTR;
        break;
    case __NR_listen:
        *etype = DIO_LISTEN;
        break;
    case __NR_bind:
        *etype = DIO_BIND;
        break;
    case __NR_accept:
        *etype = DIO_ACCEPT;
        break;
    case __NR_accept4:
        *etype = DIO_ACCEPT4;
        break;
    case __NR_connect:
        *etype = DIO_CONNECT;
        break;
    case __NR_recvfrom:
        *etype = DIO_RECVFROM;
        break;
    case __NR_recvmsg:
        *etype = DIO_RECVMSG;
        break;
    case __NR_sendto:
        *etype = DIO_SENDTO;
        break;
    case __NR_sendmsg:
        *etype = DIO_SENDMSG;
        break;
    case __NR_socket:
        *etype = DIO_SOCKET;
        break;
    case __NR_socketpair:
        *etype = DIO_SOCKETPAIR;
        break;
    case __NR_getsockopt:
        *etype = DIO_GETSOCKOPT;
        break;
    case __NR_setsockopt:
        *etype = DIO_SETSOCKOPT;
        break;
    case __NR_mknod:
        *etype = DIO_MKNOD;
        break;
    case __NR_mknodat:
        *etype = DIO_MKNODAT;
        break;
    case __NR_lseek:
        *etype = DIO_LSEEK;
        break;
    default:
        return 1;
    }
    return 0;
}

// -----STORAGE EVENTS---------------


static inline int raw_enter_function(struct bpf_raw_tracepoint_args *ctx)
{
    struct pid_info_t proc_info = pid_info();
    if (skip(proc_info)) return 1;

    int syscall_nr = (int) ctx->args[1];

    struct event_key_t key = { .pid = proc_info.kpid };
    if (get_etype(&(key.type), syscall_nr)) return 1;

    incrementEnterCounter(key.type);

    struct entry_args_t args;
    args.timestamp = bpf_ktime_get_ns();
    bpf_probe_read(&args.args, sizeof(args.args), ctx->args);
    entry_syscall_args.update(&key, &args);
    return 0;

}

static inline int raw_exit_function(struct bpf_raw_tracepoint_args *ctx) {
    struct pid_info_t proc_info = pid_info();
    if (skip(proc_info)) return 1;
    int syscall_nr = (int) ctx->args[1];
    long return_value = (long) ctx->args[2];
    struct event_key_t key = { .pid = proc_info.kpid };
    if (get_etype(&(key.type), syscall_nr)) return 1;
    if (return_value < 0) {
        incrementErrorCounter(key.type);
        if (DISCARD_ERRORS == 1) return 0;
    }

    struct entry_args_t *args = entry_syscall_args.lookup(&key);
    if (!args) return 1;
    entry_syscall_args.delete(&key);

    Event event = {};

    if (createEventBase(&event, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    for (int i = 2; i < NARGS+2; i++) {
        bpf_probe_read(&event.args[i-2], sizeof(event.args[i-2]), &args->args[i]);
    }

    handle_calls_event(key.type, event.return_time, sizeof(event));
    if (events.perf_submit((struct pt_regs *) ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.return_time, sizeof(event));
    else handle_lost_event(key.type, event.return_time, sizeof(event));

    return 0;
}

// -----PROCESS EVENTS---------------

/**
 * Handle forks.
 */
struct sched_process_fork
{
    u64 __unused__;
    char parent_comm[16];
    pid_t parent_pid;
    char child_comm[16];
    pid_t child_pid;
};

int on_fork(struct sched_process_fork * args)
{
    struct pid_info_t proc_info = pid_info();
    if (skip(proc_info)) return 1;

    incrementEnterCounter(DIO_PROCESS_FORK);

    char zero = ' ';
    u32 child_kpid = args->child_pid;
    if (CHILDS_FILTER == 1) trace_pids.insert(&child_kpid, &zero);


    #ifdef CAPTURE_PROC_EVENTS
    Event event = {};
	event.etype = DIO_PROCESS_FORK;
    event.kpid = proc_info.kpid;
    event.tgid = proc_info.tgid;
    event.ppid = proc_info.ppid;
    event.call_time = bpf_ktime_get_ns();
    bpf_probe_read(&event.comm, sizeof(event.comm), &proc_info.comm);
    bpf_probe_read(&event.args[0], sizeof(event.args[0]), &args->child_pid);

    handle_calls_event(DIO_PROCESS_FORK, event.call_time, sizeof(event));
    if (events.perf_submit((struct pt_regs*)args, &event, sizeof(event)) == 0) handle_submitted_event(DIO_PROCESS_FORK, event.call_time, sizeof(event));
    else handle_lost_event(DIO_PROCESS_FORK, event.call_time, sizeof(event));
    #endif

    return 0;
}

/**
 * Handle process termination.
 */
struct sched_process_exit
{
    u64 __unused__;
    char comm[16];
    pid_t pid;
};

int on_exit(struct sched_process_exit *args)
{
    struct pid_info_t proc_info = pid_info();
    if (skip(proc_info)) return 1;

    incrementEnterCounter(DIO_PROCESS_END);

    struct task_struct *task = (struct task_struct *)bpf_get_current_task();
    Event event = {};
	event.etype = DIO_PROCESS_END;
    event.kpid = proc_info.kpid;
    event.tgid = proc_info.tgid;
    event.ppid = proc_info.ppid;
    event.call_time = bpf_ktime_get_ns();
    event.return_value = task->exit_code >> 8;
    bpf_probe_read(&event.comm, sizeof(event.comm), &proc_info.comm);
    bpf_probe_read(&event.args[0], sizeof(event.args[0]), &args->pid);

    handle_calls_event(DIO_PROCESS_END, event.call_time, sizeof(event));
    if (events.perf_submit((struct pt_regs*)args, &event, sizeof(event)) == 0) handle_submitted_event(DIO_PROCESS_END, event.call_time, sizeof(event));
    else handle_lost_event(DIO_PROCESS_END, event.call_time, sizeof(event));

    return 0;
}


// -----PROCESS EVENTS---------------

int enter_function(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_enter_function(ctx);
}

int exit_function_open(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_base_path(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_mknod(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_rename(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_readlink(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_truncate(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_ftruncate(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_xattr(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_xattr_fd(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_fd(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_readahead(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_lseek(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_storage_data(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_storage_iov_data(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_close(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_socket(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_bind(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_listen(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_connect_accept(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_sockopt(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_sendto_recvfrom(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}

int exit_function_send_recv_msg(struct bpf_raw_tracepoint_args *ctx)
{
    return raw_exit_function(ctx);
}
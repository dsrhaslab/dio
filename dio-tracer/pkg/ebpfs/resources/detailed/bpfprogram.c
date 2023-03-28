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

#undef container_of
#define container_of(ptr, type, member) ({ const typeof( ((type *)0)->member ) *__mptr = (ptr); (type *)( (char *)__mptr - offsetof(type,member) );})

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

struct close_args_t {
    u64 timestamp;
    FDInfo file_fd;
};

struct data_args_t {
    struct entry_args_t args;
    FDInfo file_fd;
    long offset;
};

struct socket_entry_info_t {
    unsigned short family;
	union {
		struct _unix_addr_t un;
		struct _inet_addr_t in;
		struct _netlink_addr_t nl;
	};
};

struct mount {
    struct hlist_node mnt_hash;
    struct mount *mnt_parent;
    struct dentry *mnt_mountpoint;
    struct vfsmount mnt;
};

// --------------------

BPF_HASH(trace_pids, u32, char);
BPF_HASH(trace_tids, u32, char);

BPF_HASH(opened_fds, FileFDKey, u64);
BPF_HASH(entry_syscall_args, struct event_key_t, struct entry_args_t);
BPF_HASH(entry_data_buf_args, struct event_key_t, struct data_args_t);
BPF_HASH(close_file_tag, struct event_key_t, struct close_args_t);

#ifdef TRACE_SOCKADDR
BPF_HASH(sock_handlers, struct event_key_t, struct socket_entry_info_t);
#endif

#if COMPUTE_HASH==1
BPF_PERCPU_ARRAY(percpu_array_content, Message_Content, PER_CPU_ENTRIES);
#endif
BPF_PERCPU_ARRAY(percpu_array_files, FileInfo, PER_CPU_ENTRIES);

#ifdef ONE2MANY
BPF_PERCPU_ARRAY(percpu_array_events_global, struct event_t, TOTAL_EVENTS);
#endif
#ifndef ONE2MANY
BPF_PERCPU_ARRAY(percpu_array_events_storage_data, EventStorageData);
BPF_PERCPU_ARRAY(percpu_array_events_network, EventNetwork, 1);
BPF_PERCPU_ARRAY(percpu_array_events_network_accept, EventAccept, 1);
BPF_PERCPU_ARRAY(percpu_array_events_network_data, EventNetworkData, 1);
#endif

BPF_ARRAY(target_paths_array, TargetPathEntry, 30);

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

static const uint32_t prime32x1 = 2654435761;
static const uint32_t prime32x2 = 2246822519;
static const uint32_t prime32x3 = 3266489917;
static const uint32_t prime32x4 = 668265263;
static const uint32_t prime32x5 = 374761393;

static inline uint32_t rotl32_1(uint32_t x)  { return (x << 1) | (x >> (32 - 1)); }
static inline uint32_t rotl32_7(uint32_t x)  { return (x << 7) | (x >> (32 - 7)); }
static inline uint32_t rotl32_11(uint32_t x) { return (x << 11) | (x >> (32 - 11)); }
static inline uint32_t rotl32_12(uint32_t x) { return (x << 12) | (x >> (32 - 12)); }
static inline uint32_t rotl32_13(uint32_t x) { return (x << 13) | (x >> (32 - 13)); }
static inline uint32_t rotl32_17(uint32_t x) { return (x << 17) | (x >> (32 - 17)); }
static inline uint32_t rotl32_18(uint32_t x) { return (x << 18) | (x >> (32 - 18)); }

static inline uint32_t xxhash32(const uint8_t* key, ssize_t len, uint32_t seed )
{
    uint32_t h, k, val;
    int i = 0, j = 0;

    const unsigned char* data = (const unsigned char*)key;

    if (len > MAX_BUF_SIZE) return 0;
    if (len >= 16) {
        uint32_t v1 =seed + prime32x1 + prime32x2;
        uint32_t v2 =seed + prime32x2;
        uint32_t v3 =seed + 0;
        uint32_t v4 =seed - prime32x1;

		for (j=0; i <= (len-16) && j < (MAX_BUF_SIZE/16); i += 16, j++) {

            bpf_probe_read(&k, sizeof(k), data);
			v1 += k * prime32x2;
			v1 = rotl32_13(v1) * prime32x1;
            data += sizeof(uint32_t);

            bpf_probe_read(&k, sizeof(k), data);
			v2 += k * prime32x2;
			v2 = rotl32_13(v2) * prime32x1;
            data += sizeof(uint32_t);

            bpf_probe_read(&k, sizeof(k), data);
			v3 += k * prime32x2;
			v3 = rotl32_13(v3) * prime32x1;
            data += sizeof(uint32_t);

            bpf_probe_read(&k, sizeof(k), data);
			v4 += k * prime32x2;
			v4 = rotl32_13(v4) * prime32x1;
            data += sizeof(uint32_t);
		}
        h = rotl32_1(v1) + rotl32_7(v2) + rotl32_12(v3) + rotl32_18(v4);
	} else {
		h = seed + prime32x5;
	}

	h += (uint32_t)len;
	for (j=0; i <= (len-4) && (j < 16); i += 4, j++) {
        bpf_probe_read(&k, sizeof(k), data);
		h += k * prime32x3;
		h = rotl32_17(h) * prime32x4;
        data += sizeof(uint32_t);
	}

	for (j=0; i < len && j < 4; i++, j++) {
        u_int8_t val;
        bpf_probe_read(&val, sizeof(val), data);
		h += val * prime32x5;
		h = rotl32_11(h) * prime32x1;
        data++;
	}

	h ^= h >> 15;
	h *= prime32x2;
	h ^= h >> 13;
	h *= prime32x3;
	h ^= h >> 16;

	return h;
}

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

static inline int discard_path(FileInfo *fi) {
    unsigned int i, j, key, flag;
    if (S_ISREG(fi->file_type) || S_ISDIR(fi->file_type) || S_ISSOCK(fi->file_type)) {
        if (TARGET_PATHS_LEN <= 0) return 0;
    #pragma unroll
        for (i = 0; i < TARGET_PATHS_LEN; i++) {
            key = i;
            TargetPathEntry *target_path = target_paths_array.lookup(&key);
            if (!target_path) return 0;
            if (fi->size < target_path->size) continue;
            flag = 0;
            for(j = 0; j < target_path->size; j++) {
                if (j >= TARGET_PATH_LEN) break;
                if ( target_path->name[j] != fi->filename[(fi->offset+j) & (FILENAME_MAX-1)]) { flag = 1; break; }
            }
            if (flag == 0) return 0;
        }
    }
    return 1;
}

int static to_discard_path(char *path, int size)
{
    int i, j, key, flag;

    if (TARGET_PATHS_LEN <= 0) return 0;
    for (i = 0; i < TARGET_PATHS_LEN; i++) {
        key = i;
        TargetPathEntry *target_path = target_paths_array.lookup(&key);
        if (!target_path) return 0;

        if (size >= target_path->size) {
            flag = 0;
            for(j = 0; j < target_path->size; j++) {
                if (j >= size) break;
                if (j >= 60) break;
                if ( target_path->name[j] != path[j]) { flag = 1; break; }
            }
            if (flag == 0) return 0;
        }
    }
    return 1;
}

// --------------------

/**
 * @brief Increment the percpu index counter
 *
 * @param key
 * @return u64
 */
u64 static incrementIndexCounter(int key)
{
    index_counter.atomic_increment(key);
    u64 *count = index_counter.lookup(&key);
    if (count) {
        return *count;
    }
    return 0;
}

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



static inline int createEventBase(EventBase *context, enum event_type e_type, struct pid_info_t *proc_info, u64 *call_timestamp, long *return_value)
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

// ----------------------------------

/**
 * @brief Get the file struct from a given file descriptor
 *
 * @param fd
 * @return struct file*
 */
static inline struct file* get_file_from_fd(int32_t fd)
{
    if (fd < 0) return NULL;

    struct task_struct *curr_task = (struct task_struct *) bpf_get_current_task();
    if (!curr_task) return NULL;

    struct files_struct *files = NULL;
    bpf_probe_read(&files, sizeof(files), &curr_task->files);
    if (!files) return NULL;

    struct fdtable *fdtable = NULL;
    bpf_probe_read(&fdtable, sizeof(fdtable), &files->fdt);
    if (!fdtable) return NULL;

    struct file **fileptr = NULL;
    bpf_probe_read(&fileptr, sizeof(fileptr), &fdtable->fd);
    if (!fileptr) return NULL;

    struct file *file = NULL;
    bpf_probe_read(&file, sizeof(file), &fileptr[fd]);

    return file;
}

/**
 * @brief Get the dentry struct from a given file struct
 *
 * @param file
 * @return struct dentry*
 */
static inline struct dentry* get_dentry_from_file(struct file *file)
{
    struct path path;
    struct dentry* dentry = NULL;

    if (file) {
        bpf_probe_read(&path, sizeof(path), (const void*)&file->f_path);
        dentry = path.dentry;
    }
    return dentry;
}

/**
 * @brief Get the file_tag for a given inode
 *
 * @param file_tag
 * @param inode
 * @return int
 */
static inline int get_file_tag(FileFDKey *file_tag, struct inode *inode) {
    struct super_block	*i_sb = NULL;
    bpf_probe_read(&i_sb, sizeof(struct i_sb*), &inode->i_sb);
    if (!i_sb) return -1;

    bpf_probe_read(&file_tag->ino, sizeof(file_tag->ino), &inode->i_ino);
    bpf_probe_read(&file_tag->dev, sizeof(file_tag->dev), &i_sb->s_dev);

    return 0;
}

static inline struct path get_path_from_file(struct file *file) {
    struct path path;
    bpf_probe_read(&path, sizeof(path), (const void*)&file->f_path);
    return path;
}
static inline struct inode* get_inode_from_path(struct path *path ) {
    struct inode *inode;
    struct dentry *dentry = path->dentry;
    if(!dentry) return NULL;
    bpf_probe_read(&inode, sizeof(inode), &dentry->d_inode);
    return inode;
}

static inline struct dentry* get_dentry_from_path(struct path *path ) {
    return path->dentry;
}

static inline struct vfsmount* get_mnt_from_path(struct path *path ) {
    return path->mnt;
}

static inline struct mount* get_real_mount(struct vfsmount *vfs_mnt) {
    return container_of(vfs_mnt, struct mount, mnt);
}

static inline struct mount* get_mount_parent(struct mount *mnt) {
    struct mount *mount_p;
    bpf_probe_read(&mount_p, sizeof(struct mount *), &mnt->mnt_parent);
    return mount_p;
}

static inline int get_file_path(struct path *path, struct event_path_t *event_path, FileInfo *fi) {
    u32 i, len, offset, last_position;
    char slashchar = '/', nulchar = '\0';
    struct dentry *dentry, *parent, *mnt_root;
    struct vfsmount *vfs_mnt;
    struct mount *real_mount, *mount_parent;
    struct qstr d_name;
    int flag = 0;
    dentry = get_dentry_from_path(path);
    if (!dentry) return 1;
    vfs_mnt = get_mnt_from_path(path);
    if (!vfs_mnt) return 1;
    real_mount = get_real_mount(vfs_mnt);
    if (!real_mount) return 1;
    mount_parent = get_mount_parent(real_mount);
    if (!mount_parent) return 1;
    offset = last_position = MAX_FILE_OFFSET;
#pragma unroll
    for (i = 0; i < MAX_JUMPS; i++) {
        // get parent dentry
        bpf_probe_read_kernel((void *) &parent, sizeof(parent), (void*)&(dentry->d_parent));
        if (!parent) break;
        // get mount root dentry
        bpf_probe_read_kernel((void *) &mnt_root, sizeof(mnt_root), (void*)&(vfs_mnt->mnt_root));
        if (!mnt_root) break;
        // stop if dentry equals parent or mount root
        if (dentry == mnt_root || dentry == parent)  {
            // find final root through the mount point
            if (dentry == mnt_root && real_mount != mount_parent) {
                bpf_probe_read(&dentry, sizeof(dentry), &real_mount->mnt_mountpoint);
                bpf_probe_read(&real_mount, sizeof(real_mount), &real_mount->mnt_parent);
                bpf_probe_read(&mount_parent, sizeof(mount_parent), &real_mount->mnt_parent);
                vfs_mnt = &real_mount->mnt;
                continue;
            }
            // reached end -> stop
            flag=1;
            if (i>0) offset++;
        }
        // get file name length
        bpf_probe_read_kernel(&d_name, sizeof(d_name), (const void*)&(dentry->d_name));
        len = d_name.len + 1;
        if (len >= SUB_STR_MAX) len = SUB_STR_MAX-1;
        len = len & (SUB_STR_MAX-1);
        // calculate new position to write
        offset = (offset - len);
        if (offset > last_position) break;
        // copy file name into buffer
        len = (len-1) &  (MAX_FILE_OFFSET - 1);
        int err = bpf_probe_read_kernel(&(fi->filename[offset & (MAX_FILE_OFFSET-1)]), len, (void *) d_name.name);
        if (err < 0) break;
        if (flag) {
            last_position = offset;
            break;
        }
        // Add a slash character
        last_position--;
        bpf_probe_read_kernel(&(fi->filename[last_position & (FILENAME_MAX-1)]), 1, &slashchar);
        last_position = offset;
        // get parent dentry name
        dentry = parent;
    }
    fi->n_ref = event_path->n_ref;
    fi->offset = last_position;
    if (last_position == MAX_FILE_OFFSET) fi->size = MAX_FILE_OFFSET - last_position;
    else fi->size = MAX_FILE_OFFSET - last_position - 1;
    return 0;
}


static inline int check_inode(struct pt_regs *ctx, enum event_type e_type, FDInfo *base_fd, u64 *call_timestamp) {
    // 1. get file_tag (dev+inode)
    struct file *file = get_file_from_fd(base_fd->file_descriptor);
    if (!file) return 1;
    struct path path = get_path_from_file(file);
    struct inode *inode = get_inode_from_path(&path);
    if (!inode) return 2;
    if (get_file_tag(&base_fd->f_tag, inode)) return 2;

    // 2. check if file_tag is in "opened_inodes":
    u64 *i_timestamp_p = opened_fds.lookup(&base_fd->f_tag);
    if (i_timestamp_p != NULL) {
        // if i_timestamp_p is not NULL -> just add timestamp to base_fd and return 0
        base_fd->timestamp = *i_timestamp_p;
    } else {
        // 3. else get file path
        EventPath event_path = {};
        event_path.etype = DIO_PATH_EVENT;
        event_path.n_ref = incrementIndexCounter(0);
        event_path.index = (event_path.n_ref % PER_CPU_ENTRIES);
        event_path.cpu = bpf_get_smp_processor_id();

        FileInfo *fi = percpu_array_files.lookup(&event_path.index);
        if (!fi) return 2;

        bpf_probe_read(&fi->file_type, sizeof(fi->file_type), &inode->i_mode);

        if (get_file_path(&path, &event_path, fi) != 0) return 2;

        #if DISCARD_DIRECTORIES == 1
        if (S_ISDIR(fi->file_type)) {
            incrementDiscardedCounter(e_type);
            return 2;
        }
        #endif

        // 4. check path against list of target paths.
        // if path not in list of target paths -> discard base_fd
        #ifdef FILTER_FILES
        if (discard_path(fi) == 1) {
            incrementDiscardedCounter(e_type);
            return 2;
        }
        #endif

        // 5. if path is in list of target paths
        // add file_tag to opened_fds
        incrementEnterCounter(DIO_PATH_EVENT);
        event_path.f_tag = base_fd->f_tag;
        bpf_probe_read(&event_path.timestamp, sizeof(event_path.timestamp), call_timestamp);
        base_fd->timestamp = event_path.timestamp;

        opened_fds.update(&base_fd->f_tag, call_timestamp);

        // 6. submit a new "event_path" to userspace
        handle_calls_event(DIO_PATH_EVENT, event_path.timestamp, sizeof(event_path));
        if (events.perf_submit(ctx, &event_path, sizeof(event_path)) == 0) handle_submitted_event(DIO_PATH_EVENT, event_path.timestamp, sizeof(event_path));
        else handle_lost_event(DIO_PATH_EVENT, event_path.timestamp, sizeof(event_path));

    }
    return 0;
}

// ----

static inline u16 get_file_type(struct file *file) {
    struct dentry *dentry = get_dentry_from_file(file);
    if (!dentry) return -1;

    struct inode *inode = NULL;
    bpf_probe_read(&inode, sizeof(struct inode*), &dentry->d_inode);
    if (!inode) return -1;

    u16 file_type;
    bpf_probe_read(&file_type, sizeof(file_type), &inode->i_mode);

    return file_type;
}

static inline u16 get_socket_info(u32 fd, struct socket_data_t * sock_data)
{
    #ifdef TRACE_SOCKDATA
    struct file* file = NULL;

    file = get_file_from_fd(fd);
    if (!file) return -1;

    u16 file_type = get_file_type(file);

    if (S_ISSOCK(file_type)) {
        struct socket * sock;
        bpf_probe_read(&sock, sizeof(struct socket*), &file->private_data);
        if (sock) {
            struct sock *sk;
            bpf_probe_read(&sk, sizeof(struct sock*), &sock->sk);
            if (sk) {
                bpf_probe_read(&sock_data->family, sizeof(sock_data->family), &(sk->sk_family));
                bpf_probe_read(&sock_data->sport, sizeof(sock_data->sport), &(sk->sk_num));
                bpf_probe_read(&sock_data->dport, sizeof(sock_data->dport), &(sk->sk_dport));
                sock_data->dport = ntohs(sock_data->dport);

                if (sock_data->family == AF_INET) {
                    bpf_probe_read(&sock_data->saddr[1], sizeof(uint32_t), &sk->sk_rcv_saddr);
                    bpf_probe_read(&sock_data->daddr[1], sizeof(uint32_t), &sk->sk_daddr);
                } else if (sock_data->family == AF_INET6) {
                    bpf_probe_read(sock_data->saddr, sizeof(sock_data->saddr), &(sk->sk_v6_rcv_saddr));
                    bpf_probe_read(sock_data->daddr, sizeof(sock_data->daddr), &(sk->sk_v6_daddr));
                }
                return file_type;
            }
        }
    }
    #endif
    return 0;
}

static inline int getNetworkDataFromSock(struct sockaddr * sk, u32 addr_len, EventNetwork *event)
{
    #ifdef TRACE_SOCKADDR
    bpf_probe_read(&event->family, sizeof(event->family), &(sk->sa_family));
    bpf_probe_read(&event->addr_len, sizeof(event->addr_len), &addr_len);
    if (event->family == AF_UNIX) {
        struct sockaddr_un *sock = (struct sockaddr_un *)sk;
        bpf_probe_read(&event->un.path, sizeof(event->un.path), sock->sun_path);
    } else if (event->family == AF_INET) {
        struct sockaddr_in *sock = (struct sockaddr_in *)sk;
        bpf_probe_read(&event->in.port, sizeof(event->in.port), &(sock->sin_port));
        bpf_probe_read(&event->in.addr[1], sizeof(uint32_t), &(sock->sin_addr.s_addr));
        event->in.port = ntohs(event->in.port);
    } else if (event->family == AF_INET6) {
        struct sockaddr_in6 *sock = (struct sockaddr_in6 *)sk;
        bpf_probe_read(&event->in.port, sizeof(event->in.port), &(sock->sin6_port));
        bpf_probe_read(event->in.addr, sizeof(event->in.addr), &(sock->sin6_addr));
        event->in.port = ntohs(event->in.port);
    } else if (event->family == AF_NETLINK) {
        struct sockaddr_nl *sock = (struct sockaddr_nl *)sk;
        bpf_probe_read(&event->nl.port_id, sizeof(event->nl.port_id), &(sock->nl_pid));
        bpf_probe_read(&event->nl.groups, sizeof(event->nl.groups), &(sock->nl_groups));
    }
    #endif
    return 0;
}

static int copyDataToPerCpu(DataInfo *event, char *buf, uint64_t len)
{
    #if COMPUTE_HASH==1
    u64 event_ref = incrementIndexCounter(1);
    event->index = (event_ref % PER_CPU_ENTRIES);
    event->n_ref = event_ref;

    Message_Content* content = percpu_array_content.lookup(&event->index);
    if (content != NULL) {
        content->n_ref = event_ref;
        if (len < 0 || len > MAX_BUF_SIZE) return 1;
        bpf_probe_read(&(content->msg), len, buf);
    }
    #endif
    return 0;
}

// -----STORAGE EVENTS---------------


int enter_function(struct bpf_raw_tracepoint_args *ctx)
{
    struct pid_info_t proc_info = pid_info();
    if (skip(proc_info)) return 1;

    int syscall_nr = (int) ctx->args[1];

    struct event_key_t key = { .pid = proc_info.kpid };
    if (get_etype(&(key.type), syscall_nr)) return 1;

    incrementEnterCounter(key.type);

    switch (key.type)
    {
        case DIO_CREAT:
        case DIO_OPEN:
        case DIO_OPENAT:
        case DIO_FSYNC:
        case DIO_FDATASYNC:
        case DIO_RENAME:
        case DIO_RENAMEAT:
        case DIO_RENAMEAT2:
        case DIO_READLINK:
        case DIO_READLINKAT:
        case DIO_TRUNCATE:
        case DIO_FTRUNCATE:
        case DIO_UNLINK:
        case DIO_UNLINKAT:
        case DIO_STAT:
        case DIO_LSTAT:
        case DIO_FSTAT:
        case DIO_FSTATAT:
        case DIO_FSTATFS:
        case DIO_LISTXATTR:
        case DIO_LLISTXATTR:
        case DIO_FLISTXATTR:
        case DIO_GETXATTR:
        case DIO_LGETXATTR:
        case DIO_FGETXATTR:
        case DIO_SETXATTR:
        case DIO_LSETXATTR:
        case DIO_FSETXATTR:
        case DIO_REMOVEXATTR:
        case DIO_LREMOVEXATTR:
        case DIO_FREMOVEXATTR:
        case DIO_READAHEAD:
        case DIO_SOCKET:
        case DIO_SOCKETPAIR:
        case DIO_BIND:
        case DIO_LISTEN:
        case DIO_ACCEPT:
        case DIO_ACCEPT4:
        case DIO_CONNECT:
        case DIO_GETSOCKOPT:
        case DIO_SETSOCKOPT:
        case DIO_MKNOD:
        case DIO_MKNODAT:
        case DIO_LSEEK:
        {
            struct entry_args_t args;
            args.timestamp = bpf_ktime_get_ns();
            bpf_probe_read(&args.args, sizeof(args.args), ctx->args);
            entry_syscall_args.update(&key, &args);
            return 0;
        }
        case DIO_CLOSE:
        {
            struct close_args_t c_args = {};
            bpf_probe_read(&c_args.file_fd.file_descriptor, sizeof(c_args.file_fd.file_descriptor), &ctx->args[2]);
            c_args.timestamp = bpf_ktime_get_ns();

            if (check_inode((struct pt_regs *) ctx, key.type, &c_args.file_fd, &c_args.timestamp) > 1) return 1;

            close_file_tag.update(&key, &c_args);
            return 0;
        }
        case DIO_WRITE:
        case DIO_READ:
        case DIO_READV:
        case DIO_WRITEV:
        {
            struct data_args_t aux_data = {};
            aux_data.args.timestamp = bpf_ktime_get_ns();
            bpf_probe_read(&aux_data.file_fd.file_descriptor, sizeof(aux_data.file_fd.file_descriptor), &ctx->args[2]);
            bpf_probe_read(&aux_data.args.args, sizeof(aux_data.args.args), ctx->args);
            if (check_inode((struct pt_regs*)ctx, key.type, &aux_data.file_fd, &aux_data.args.timestamp) > 1) return 1;
            struct file *file = get_file_from_fd(aux_data.file_fd.file_descriptor);
            if (file) {
                bpf_probe_read(&aux_data.offset, sizeof(aux_data.offset), &file->f_pos);
            } else {
                long pos = -1;
                bpf_probe_read(&aux_data.offset, sizeof(aux_data.offset), &pos);
            }
            entry_data_buf_args.update(&key, &aux_data);
            return 0;
        }
        case DIO_PREAD64:
        case DIO_PWRITE64:
        {
            struct data_args_t aux_data = {};
            aux_data.args.timestamp = bpf_ktime_get_ns();
            bpf_probe_read(&aux_data.file_fd.file_descriptor, sizeof(aux_data.file_fd.file_descriptor), &ctx->args[2]);
            bpf_probe_read(&aux_data.args.args, sizeof(aux_data.args.args), ctx->args);
            if (check_inode((struct pt_regs*)ctx, key.type, &aux_data.file_fd, &aux_data.args.timestamp) > 1) return 1;
            bpf_probe_read(&aux_data.offset, sizeof(aux_data.offset), &ctx->args[5]);
            entry_data_buf_args.update(&key, &aux_data);
            return 0;
        }
        case DIO_RECVFROM:
        case DIO_SENDTO:
        case DIO_RECVMSG:
        case DIO_SENDMSG:
        {
            struct data_args_t aux_data = {};
            aux_data.args.timestamp = bpf_ktime_get_ns();
            int pos = -1;
            bpf_probe_read(&aux_data.offset, sizeof(aux_data.offset), &pos);
            bpf_probe_read(&aux_data.file_fd.file_descriptor, sizeof(aux_data.file_fd.file_descriptor), &ctx->args[2]);
            bpf_probe_read(&aux_data.args.args, sizeof(aux_data.args.args), ctx->args);
            if (check_inode((struct pt_regs*)ctx, key.type, &aux_data.file_fd, &aux_data.args.timestamp) > 1) return 1;
            entry_data_buf_args.update(&key, &aux_data);
            return 0;
        }
        default:
            break;
    }


    return 1;
}

int exit_function_open(struct bpf_raw_tracepoint_args *ctx) {
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

    EventOpen event = {};

    if (createEventBase(&event.base_fd_info.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    if (return_value >= 0) {
        event.base_fd_info.file_fd.file_descriptor = return_value;
        if (check_inode((struct pt_regs *) ctx, key.type, &event.base_fd_info.file_fd, &event.base_fd_info.base.call_time) > 1) return 1;
    }

    if (key.type == DIO_CREAT) {
        bpf_probe_read(&event.mode, sizeof(event.mode), &args->args[3]);
        event.flags = O_CREAT|O_WRONLY|O_TRUNC;
    } else if (key.type == DIO_OPEN) {
        bpf_probe_read(&event.flags, sizeof(event.flags), &args->args[3]);
        bpf_probe_read(&event.mode, sizeof(event.mode), &args->args[4]);
    } else if (key.type == DIO_OPENAT) {
        bpf_probe_read(&event.flags, sizeof(event.flags), &args->args[4]);
        bpf_probe_read(&event.mode, sizeof(event.mode), &args->args[5]);
    }

    handle_calls_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    if (events.perf_submit((struct pt_regs *) ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base_fd_info.base.return_time, sizeof(event));

    return 0;
}

int exit_function_base_path(struct bpf_raw_tracepoint_args *ctx) {
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

    EventBasePath event = {};
    if (createEventBase(&event.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;


    #ifdef CAPTURE_ARG_PATHS
    event.n_ref = incrementIndexCounter(0);
    event.index = (event.n_ref % PER_CPU_ENTRIES);
    event.cpu = bpf_get_smp_processor_id();
    FileInfo *fi = percpu_array_files.lookup(&event.index);
    if (fi) {
        fi->n_ref = event.n_ref;
        u32 size=0;
        if ((key.type == DIO_UNLINKAT) || (key.type == DIO_FSTATAT) || (key.type == DIO_READLINKAT)) {
            size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[3]);
        } else {
            size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[2]);
        }

        if (size > 0) fi->size = size-1;
        if (to_discard_path(&(fi->filename[0]), fi->size) == 1) return 1;
    }
    #endif

    if (key.type == DIO_UNLINKAT) bpf_probe_read(&event.flag, sizeof(event.flag), (int*)&args->args[4]);
    else if (key.type == DIO_FSTATAT) bpf_probe_read(&event.flag, sizeof(event.flag), (int*)&args->args[5]);

    handle_calls_event(key.type, event.base.return_time, sizeof(event));
    if (events.perf_submit(ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base.return_time, sizeof(event));
    return 0;
}

int exit_function_mknod(struct bpf_raw_tracepoint_args *ctx) {
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

    EventMknod event = {};
    if (createEventBase(&event.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;


    #ifdef CAPTURE_ARG_PATHS
    event.n_ref = incrementIndexCounter(0);
    event.index = (event.n_ref % PER_CPU_ENTRIES);
    event.cpu = bpf_get_smp_processor_id();
    FileInfo *fi = percpu_array_files.lookup(&event.index);
    if (fi) {
        fi->n_ref = event.n_ref;
        int size=0;
        if ( key.type == DIO_MKNOD ) {
            size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[2]);
        } else {
            size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[3]);
        }

        if (size > 0) fi->size = size-1;
        if (to_discard_path(&(fi->filename[0]), fi->size) == 1) return 1;
    }
    #endif

    if (key.type == DIO_MKNOD) {
        bpf_probe_read(&event.mode, sizeof(event.mode), (uint16_t*)&args->args[3]);
        bpf_probe_read(&event.dev, sizeof(event.dev), (uint16_t*)&args->args[4]);
    } else {
        bpf_probe_read(&event.mode, sizeof(event.mode), (uint16_t*)&args->args[4]);
        bpf_probe_read(&event.dev, sizeof(event.dev), (uint16_t*)&args->args[5]);
    }

    handle_calls_event(key.type, event.base.return_time, sizeof(event));
    if (events.perf_submit(ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base.return_time, sizeof(event));
    return 0;
}

int exit_function_rename(struct bpf_raw_tracepoint_args *ctx) {
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

    Event2Paths event = {};
    if (createEventBase(&event.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    #ifdef CAPTURE_ARG_PATHS
    event.n_ref = incrementIndexCounter(0);
    event.index = (event.n_ref % PER_CPU_ENTRIES);
    event.cpu = bpf_get_smp_processor_id();
    FileInfo *fi = percpu_array_files.lookup(&event.index);
    if (fi) {
        fi->n_ref = event.n_ref;
        int size, discard = 0;

        if (key.type == DIO_RENAME) size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[2]);
        else size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[3]);
        if (size > 0) event.oldname_len = size-1;
        if (size > MAX_FILE_OFFSET) event.oldname_len = MAX_FILE_OFFSET;
        if (to_discard_path(&(fi->filename[0]), event.oldname_len) == 1) discard++;

        if (key.type == DIO_RENAME) size = bpf_probe_read_str(&fi->filename[event.oldname_len&(MAX_FILE_OFFSET-1)], MAX_FILE_OFFSET, (char*)args->args[3]);
        else size = bpf_probe_read_str(&fi->filename[event.oldname_len&(MAX_FILE_OFFSET-1)], MAX_FILE_OFFSET, (char*)args->args[5]);
        if (size > 0) event.newname_len = size-1;
        if (to_discard_path(&fi->filename[event.oldname_len&(MAX_FILE_OFFSET-1)], event.newname_len) == 1) discard++;

        if (discard==2) {
            incrementDiscardedCounter(key.type);
            return 1;
        }
    }
    #endif

    if (key.type == DIO_RENAMEAT2) {
        bpf_probe_read(&event.flags, sizeof(event.flags), (unsigned int*)&args->args[6]);
    }
    handle_calls_event(key.type, event.base.return_time, sizeof(event));
    if (events.perf_submit(ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base.return_time, sizeof(event));
    return 0;
}

int exit_function_readlink(struct bpf_raw_tracepoint_args *ctx) {
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

    Event2Paths event = {};
    if (createEventBase(&event.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    #ifdef CAPTURE_ARG_PATHS
    event.n_ref = incrementIndexCounter(0);
    event.index = (event.n_ref % PER_CPU_ENTRIES);
    event.cpu = bpf_get_smp_processor_id();
    FileInfo *fi = percpu_array_files.lookup(&event.index);
    if (fi) {
        fi->n_ref = event.n_ref;
        int size, discard = 0;
        if (key.type == DIO_READLINK) size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[2]);
        else size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[3]);
        if (size > 0)  event.oldname_len = (size-1);
        if (size > MAX_FILE_OFFSET) event.oldname_len = MAX_FILE_OFFSET;
        if (to_discard_path(&(fi->filename[0]), event.oldname_len) == 1) discard++;

        event.oldname_len &= (MAX_FILE_OFFSET-1);
        if (key.type == DIO_READLINK) size = bpf_probe_read_str(&(fi->filename[event.oldname_len&(MAX_FILE_OFFSET-1)]), MAX_FILE_OFFSET, (char*)args->args[3]);
        else size = bpf_probe_read_str(&(fi->filename[event.oldname_len&(MAX_FILE_OFFSET-1)]), MAX_FILE_OFFSET, (char*)args->args[4]);
        if (size > 0) event.newname_len = size-1;
        if (to_discard_path(&(fi->filename[event.oldname_len&(MAX_FILE_OFFSET-1)]), event.newname_len) == 1) discard++;
        if (discard==2) {
            incrementDiscardedCounter(key.type);
            return 1;
        }
    }
    #endif

    handle_calls_event(key.type, event.base.return_time, sizeof(event));
    if (events.perf_submit(ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base.return_time, sizeof(event));
    return 0;
}

int exit_function_truncate(struct bpf_raw_tracepoint_args *ctx) {
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

    EventTruncate event = {};
    if (createEventBase(&event.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    #ifdef CAPTURE_ARG_PATHS
    event.n_ref = incrementIndexCounter(0);
    event.index = (event.n_ref % PER_CPU_ENTRIES);
    event.cpu = bpf_get_smp_processor_id();
    FileInfo *fi = percpu_array_files.lookup(&event.index);
    if (fi) {
        fi->n_ref = event.n_ref;
        int size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[2]);
        if (size > 0) fi->size = size-1;
        if (to_discard_path(&(fi->filename[0]), fi->size) == 1) return 1;
    }
    #endif

    bpf_probe_read(&event.length, sizeof(event.length), &args->args[3]);
    handle_calls_event(key.type, event.base.return_time, sizeof(event));
    if (events.perf_submit(ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base.return_time, sizeof(event));
    return 0;
}
int exit_function_ftruncate(struct bpf_raw_tracepoint_args *ctx) {
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

    EventFTruncate event = {};
    if (createEventBase(&event.base_fd_info.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    bpf_probe_read(&event.base_fd_info.file_fd.file_descriptor, sizeof(event.base_fd_info.file_fd.file_descriptor), (unsigned int*)&args->args[2]);
    if (check_inode((struct pt_regs *) ctx, key.type, &event.base_fd_info.file_fd, &(args->timestamp)) > 1) return 1;

    bpf_probe_read(&event.length, sizeof(event.length), &args->args[3]);
    handle_calls_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    if (events.perf_submit(ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    return 0;
}

int exit_function_xattr(struct bpf_raw_tracepoint_args *ctx) {
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

    EventXattr event = {};
    if (createEventBase(&event.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    #ifdef CAPTURE_ARG_PATHS
    event.n_ref = incrementIndexCounter(0);
    event.index = (event.n_ref % PER_CPU_ENTRIES);
    event.cpu = bpf_get_smp_processor_id();
    FileInfo *fi = percpu_array_files.lookup(&event.index);
    if (fi) {
        fi->n_ref = event.n_ref;
        int size = bpf_probe_read_str(&(fi->filename[0]), MAX_FILE_OFFSET, (char*)args->args[2]);
        if (size > 0) fi->size = size-1;
        if (to_discard_path(&(fi->filename[0]), fi->size) == 1) return 1;
    }
    #endif

    bpf_probe_read_str(event.name, MAX_ATTR_NAME_SIZE, (char*)args->args[3]);

    if ((key.type == DIO_SETXATTR) || (key.type == DIO_LSETXATTR)) {
        bpf_probe_read(&event.flag, sizeof(event.flag), (int*)&args->args[6]);
    }

    handle_calls_event(key.type, event.base.return_time, sizeof(event));
    if (events.perf_submit(ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base.return_time, sizeof(event));
    return 0;
}
int exit_function_xattr_fd(struct bpf_raw_tracepoint_args *ctx) {
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

    EventXattrFD event = {};
    if (createEventBase(&event.base_fd_info.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    bpf_probe_read(&event.base_fd_info.file_fd.file_descriptor, sizeof(event.base_fd_info.file_fd.file_descriptor), (unsigned int*)&args->args[2]);
    if (check_inode((struct pt_regs *) ctx, key.type, &event.base_fd_info.file_fd, &(args->timestamp)) > 1) return 1;

    bpf_probe_read_str(event.name, MAX_ATTR_NAME_SIZE, (char*)args->args[3]);

    if (key.type == DIO_FSETXATTR) {
        bpf_probe_read(&event.flag, sizeof(event.flag), (int*)&args->args[6]);
    }

    handle_calls_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    if (events.perf_submit(ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    return 0;
}

int exit_function_fd(struct bpf_raw_tracepoint_args *ctx) {
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

    struct event_base_fd_t event = {};
    if (createEventBase(&event.base, key.type, &proc_info, &(args->timestamp), &return_value)) return 1;
    bpf_probe_read(&event.file_fd.file_descriptor, sizeof(event.file_fd.file_descriptor), (unsigned int*)&args->args[2]);

    if (check_inode((struct pt_regs *) ctx, key.type, &event.file_fd, &(args->timestamp)) > 1) return 1;

    handle_calls_event(key.type, event.base.return_time, sizeof(event));
    if (events.perf_submit((struct pt_regs *) ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base.return_time, sizeof(event));

    return 0;
}

int exit_function_readahead(struct bpf_raw_tracepoint_args *ctx) {
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

    EventReadahead event = {};
    if (createEventBase(&event.base_fd_info.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    bpf_probe_read(&event.base_fd_info.file_fd, sizeof(event.base_fd_info.file_fd), &args->args[2]);

    if (check_inode((struct pt_regs *) ctx, key.type, &event.base_fd_info.file_fd, &event.base_fd_info.base.call_time) > 1) return 1;

    bpf_probe_read(&event.offset, sizeof(event.offset), &args->args[3]);
    bpf_probe_read(&event.count, sizeof(event.count), &args->args[4]);

    handle_calls_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    if (events.perf_submit((struct pt_regs *) ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base_fd_info.base.return_time, sizeof(event));

    return 0;
}

int exit_function_lseek(struct bpf_raw_tracepoint_args *ctx) {
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

    EventLSeek event = {};
    if (createEventBase(&event.base_fd_info.base, key.type, &proc_info, &args->timestamp, &return_value)) return 1;

    bpf_probe_read(&event.base_fd_info.file_fd, sizeof(event.base_fd_info.file_fd), &args->args[2]);
    if (check_inode((struct pt_regs *) ctx, key.type, &event.base_fd_info.file_fd, &event.base_fd_info.base.call_time) > 1) return 1;

    bpf_probe_read(&event.offset, sizeof(event.offset), &args->args[3]);
    bpf_probe_read(&event.whence, sizeof(event.whence), &args->args[4]);

    handle_calls_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    if (events.perf_submit((struct pt_regs *) ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base_fd_info.base.return_time, sizeof(event));

    return 0;
}

static void getStorageDataFromBuf(char **buf, u64 *len, u64 *count, struct entry_args_t *args) {
    bpf_probe_read(buf, sizeof(*buf), &args->args[3]);
    bpf_probe_read(len, sizeof(*len), &args->args[4]);
    bpf_probe_read(count, sizeof(*count), &args->args[4]);
}
static int submit_storage_data_event(struct pt_regs *ctx, EventStorageData *event, enum event_type e_type, struct pid_info_t *proc_info, struct event_key_t *key,  long return_value)
{
    struct data_args_t *buffer_data = entry_data_buf_args.lookup(key);
    if (!buffer_data) return 1;
    entry_data_buf_args.delete(key);

    if (createEventBase(&event->base_fd_info.base, e_type, proc_info, &(buffer_data->args.timestamp), &return_value)) return 1;
    event->base_fd_info.file_fd = buffer_data->file_fd;

    char *buf;
    uint64_t len;

    getStorageDataFromBuf(&buf, &len, &event->data.bytes_request, &buffer_data->args);

    if (return_value < len) len = return_value;
    if (len >= MAX_BUF_SIZE) len = MAX_BUF_SIZE;

    event->data.offset = buffer_data->offset;

    #if COMPUTE_HASH==1
    event->data.captured_size = len;
    if (copyDataToPerCpu(&event->data, buf, event->data.captured_size)) return 1;
    #endif

    #if COMPUTE_HASH==2
    event->data.captured_size = len;
    event->data.hash = xxhash32(buf, event->data.captured_size, 12345);
    #endif

    event->file_type = get_socket_info(event->base_fd_info.file_fd.file_descriptor, &event->sock_data);

    handle_calls_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));
    if (events.perf_submit(ctx, event, sizeof(*event)) == 0) handle_submitted_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));
    else handle_lost_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));

    return 0;
}
int exit_function_storage_data(struct bpf_raw_tracepoint_args *ctx)
{
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

    #ifdef ONE2MANY
    struct event_t *event = percpu_array_events_global.lookup((int*)&key.type);
    if (!event) return 1;
    return submit_storage_data_event((struct pt_regs *) ctx, &event->event_storage_data, key.type, &proc_info, &key, return_value);
    #endif

    #ifndef ONE2MANY
    int event_key = 0;
    EventStorageData *event = percpu_array_events_storage_data.lookup(&event_key);
    if (!event) return 1;
    return submit_storage_data_event((struct pt_regs *) ctx, event, key.type, &proc_info, &key, return_value);
    #endif

    return 0;
}

static void getStorageDataFromIov(char **buf, u64 *len, u64 *count, struct entry_args_t *args) {
    struct iovec *iovec;
    bpf_probe_read(&iovec, sizeof(iovec), &args->args[3]);

    size_t vlen;
    bpf_probe_read(&vlen, sizeof(vlen), &args->args[4]);

    size_t total_bytes_request = 0;
    if (vlen > 1) {
        int i = 0;
        size_t bytes_request = 0;
        while (i < vlen) {
            if (i > 1024) break;
            bpf_probe_read(&bytes_request, sizeof(bytes_request), &iovec[i].iov_len);
            total_bytes_request += bytes_request;
            i++;
        }
    } else {
        bpf_probe_read(&total_bytes_request, sizeof(total_bytes_request), &iovec->iov_len);
    }
    bpf_probe_read(buf, sizeof(*buf), &iovec[0].iov_base);
    bpf_probe_read(count, sizeof(*count), &iovec[0].iov_len);
    bpf_probe_read(len, sizeof(*len), &total_bytes_request);
}
static int submit_storage_data_iov_event(struct pt_regs *ctx, EventStorageData *event, enum event_type e_type, struct pid_info_t *proc_info, struct event_key_t *key,  long return_value)
{
    struct data_args_t *buffer_data = entry_data_buf_args.lookup(key);
    if (!buffer_data) return 1;
    entry_data_buf_args.delete(key);

    if (createEventBase(&event->base_fd_info.base, e_type, proc_info, &(buffer_data->args.timestamp), &return_value)) return 1;
    event->base_fd_info.file_fd = buffer_data->file_fd;

    char *buf;
    uint64_t len;

    getStorageDataFromIov(&buf, &len, &event->data.bytes_request, &buffer_data->args);

    if (return_value < len) len = return_value;
    if (len >= MAX_BUF_SIZE) len = MAX_BUF_SIZE;

    event->data.offset = buffer_data->offset;

    #if COMPUTE_HASH==1
    event->data.captured_size = len;
    if (copyDataToPerCpu(&event->data, buf, event->data.captured_size)) return 1;
    #endif

    #if COMPUTE_HASH==2
    event->data.captured_size = len;
    event->data.hash = xxhash32(buf, event->data.captured_size, 12345);
    #endif

    event->file_type = get_socket_info(event->base_fd_info.file_fd.file_descriptor, &event->sock_data);

    handle_calls_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));
    if (events.perf_submit(ctx, event, sizeof(*event)) == 0) handle_submitted_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));
    else handle_lost_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));

    return 0;
}
int exit_function_storage_iov_data(struct bpf_raw_tracepoint_args *ctx)
{
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

    #ifdef ONE2MANY
    struct event_t *event = percpu_array_events_global.lookup((int*)&key.type);
    if (!event) return 1;
    return submit_storage_data_iov_event((struct pt_regs *) ctx, &event->event_storage_data, key.type, &proc_info, &key, return_value);
    #endif

    #ifndef ONE2MANY
    int event_key = 0;
    EventStorageData *event = percpu_array_events_storage_data.lookup(&event_key);
    if (!event) return 1;
    return submit_storage_data_iov_event((struct pt_regs *) ctx, event, key.type, &proc_info, &key, return_value);
    #endif

    return 0;
}

int exit_function_close(struct bpf_raw_tracepoint_args *ctx) {
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

    struct close_args_t *c_args = close_file_tag.lookup(&key);
    if (!c_args) return 1;
    close_file_tag.delete(&key);

    struct event_base_fd_t event = {};
    if (createEventBase(&event.base, key.type, &proc_info, &(c_args->timestamp), &return_value)) return 1;

    event.file_fd = c_args->file_fd;

    handle_calls_event(key.type, event.base.return_time, sizeof(event));
    if (events.perf_submit(ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base.return_time, sizeof(event));

    return 0;
}

int exit_function_socket(struct bpf_raw_tracepoint_args *ctx) {
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

    EventSocket event = {};
    if (createEventBase(&event.base_fd_info.base, key.type, &proc_info, &(args->timestamp), &return_value)) return 1;

    if (return_value >= 0) {
        if (key.type == DIO_SOCKETPAIR) {
            bpf_probe_read(&event.base_fd_info.file_fd.file_descriptor, sizeof(int), &(((int*)args->args[5])[0]));
            bpf_probe_read(&event.second_fd, sizeof(int), &(((int*)args->args[5])[1]));
        } else {
            bpf_probe_read(&event.base_fd_info.file_fd.file_descriptor, sizeof(event.base_fd_info.file_fd.file_descriptor), &return_value);
        }
        if (check_inode((struct pt_regs *)ctx, key.type, &event.base_fd_info.file_fd, &event.base_fd_info.base.call_time) > 1) return 1;
    }

    bpf_probe_read(&event.s_family, sizeof(event.s_family), (int*) &args->args[2]);
    bpf_probe_read(&event.s_type, sizeof(event.s_type), (int*) &args->args[3]);
    bpf_probe_read(&event.s_protocol, sizeof(event.s_protocol), (int*) &args->args[4]);

    handle_calls_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    if (events.perf_submit((struct pt_regs *)ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base_fd_info.base.return_time, sizeof(event));

    return 0;
}

static inline int submit_network_bind_event(struct pt_regs *ctx, EventNetwork *event, enum event_type e_type, struct pid_info_t *proc_info, struct event_key_t *key, long return_value)
{
    struct entry_args_t *args = entry_syscall_args.lookup(key);
    if (!args) return 1;
    entry_syscall_args.delete(key);

    if (createEventBase(&event->base_fd_info.base, e_type, proc_info, &(args->timestamp), &return_value)) return 1;
    bpf_probe_read(&event->base_fd_info.file_fd.file_descriptor, sizeof(event->base_fd_info.file_fd), (int*)&args->args[2]);

    if (check_inode(ctx, e_type, &event->base_fd_info.file_fd, &event->base_fd_info.base.call_time) > 1) return 1;

    #ifdef TRACE_SOCKADDR
    struct sockaddr *sk;
    bpf_probe_read(&sk, sizeof(sk), &args->args[3]);
    if (!sk) return 1;

    u16 port;
    bpf_probe_read(&event->family, sizeof(event->family), &(sk->sa_family));
    bpf_probe_read(&event->addr_len, sizeof(event->addr_len), &args->args[4]);

    if (event->family == AF_UNIX) {
        struct sockaddr_un *sock = (struct sockaddr_un *)sk;
        bpf_probe_read(&event->un.path, sizeof(event->un.path), sock->sun_path);
    } else if (event->family == AF_INET) {
        u32 temp_addr;
        struct sockaddr_in *sock = (struct sockaddr_in *)sk;
        bpf_probe_read(&port, sizeof(port), &(sock->sin_port));
        bpf_probe_read(&temp_addr, sizeof(temp_addr), &(sock->sin_addr.s_addr));
        event->in.addr[1] = temp_addr;
        event->in.port = ntohs(port);
    } else if (event->family == AF_INET6) {
        struct sockaddr_in6 *sock = (struct sockaddr_in6 *)sk;
        bpf_probe_read(&port, sizeof(port), &(sock->sin6_port));
        bpf_probe_read(event->in.addr, sizeof(event->in.addr), &(sock->sin6_addr));
        event->in.port = ntohs(port);
    } else if (event->family == AF_NETLINK) {
        struct sockaddr_nl *sock = (struct sockaddr_nl *)sk;
        bpf_probe_read(&event->nl.port_id, sizeof(event->nl.port_id), &(sock->nl_pid));
        bpf_probe_read(&event->nl.groups, sizeof(event->nl.groups), &(sock->nl_groups));
    }
    #endif

    handle_calls_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));
    if (events.perf_submit(ctx, event, sizeof(*event)) == 0) handle_submitted_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));
    else handle_lost_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));

    return 0;
}
int exit_function_bind(struct bpf_raw_tracepoint_args *ctx) {
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
    #ifdef ONE2MANY
    struct event_t *event = percpu_array_events_global.lookup((int*)&key.type);
    if (!event) return 1;
    return submit_network_bind_event((struct pt_regs *) ctx, &event->event_network, key.type, &proc_info, &key, return_value);
    #endif
    #ifndef ONE2MANY
    int event_key = 0;
    EventNetwork *event = percpu_array_events_network.lookup(&event_key);
    if (!event) return 1;
    return submit_network_bind_event((struct pt_regs *) ctx, event, key.type, &proc_info, &key, return_value);
    #endif
}

int exit_function_listen(struct bpf_raw_tracepoint_args *ctx) {
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

    struct event_network_listen_t event = {};
    if (createEventBase(&event.base_fd_info.base, key.type, &proc_info, &(args->timestamp), &return_value)) return 1;
    bpf_probe_read(&event.base_fd_info.file_fd.file_descriptor, sizeof(event.base_fd_info.file_fd.file_descriptor), (unsigned int*)&args->args[2]);
    bpf_probe_read(&event.backlog, sizeof(event.backlog), (unsigned int*)&args->args[3]);

    if (check_inode((struct pt_regs *) ctx, key.type, &event.base_fd_info.file_fd, &(args->timestamp)) > 1) return 1;

    handle_calls_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    if (events.perf_submit((struct pt_regs *) ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base_fd_info.base.return_time, sizeof(event));

    return 0;
}

static inline int submit_network_accept_event(struct pt_regs *ctx, EventAccept *event, enum event_type e_type, struct pid_info_t *proc_info, struct event_key_t *key, long return_value)
{
    struct entry_args_t *args = entry_syscall_args.lookup(key);
    if (!args) return 1;
    entry_syscall_args.delete(key);

    if (createEventBase(&event->base_fd_info.base, e_type, proc_info, &(args->timestamp), &return_value)) return 1;
    if (e_type == DIO_CONNECT) {
        bpf_probe_read(&event->base_fd_info.file_fd.file_descriptor, sizeof(event->base_fd_info.file_fd), (int*) &args->args[2]);
        if (check_inode(ctx, e_type, &event->base_fd_info.file_fd, &event->base_fd_info.base.call_time) > 1) return 1;
        get_socket_info(event->base_fd_info.file_fd.file_descriptor, &event->sock_data);
    } else {
        bpf_probe_read(&event->base_fd_info.file_fd.file_descriptor, sizeof(event->base_fd_info.file_fd.file_descriptor), &return_value);
        if (check_inode(ctx, e_type, &event->base_fd_info.file_fd, &event->base_fd_info.base.call_time) > 1) return 1;
        get_socket_info(event->base_fd_info.file_fd.file_descriptor, &event->sock_data);
        bpf_probe_read(&event->base_fd_info.file_fd.file_descriptor, sizeof(event->base_fd_info.file_fd.file_descriptor), (int*) &args->args[2]);
    }

    #ifdef TRACE_SOCKADDR
    struct sockaddr *sk;
    bpf_probe_read(&sk, sizeof(sk), &args->args[3]);
    if (!sk) return 1;

    bpf_probe_read(&event->family, sizeof(event->family), &(sk->sa_family));
    if (e_type == DIO_CONNECT) bpf_probe_read(&event->addr_len, sizeof(event->addr_len), &args->args[4]);
    else bpf_probe_read(&event->addr_len, sizeof(event->addr_len), (int*)args->args[4]);

    if (event->family == AF_UNIX) {
        struct sockaddr_un *sock = (struct sockaddr_un *)sk;
        bpf_probe_read(&event->un.path, sizeof(event->un.path), sock->sun_path);
    } else if (event->family == AF_INET) {
        struct sockaddr_in *sock = (struct sockaddr_in *)sk;
        bpf_probe_read(&event->in.port, sizeof(event->in.port), &(sock->sin_port));
        bpf_probe_read(&event->in.addr[1], sizeof(uint32_t), &(sock->sin_addr.s_addr));
        event->in.port = ntohs(event->in.port);
    } else if (event->family == AF_INET6) {
        struct sockaddr_in6 *sock = (struct sockaddr_in6 *)sk;
        bpf_probe_read(&event->in.port, sizeof(event->in.port), &(sock->sin6_port));
        bpf_probe_read(event->in.addr, sizeof(event->in.addr), &(sock->sin6_addr));
        event->in.port = ntohs(event->in.port);
    } else if (event->family == AF_NETLINK) {
        struct sockaddr_nl *sock = (struct sockaddr_nl *)sk;
        bpf_probe_read(&event->nl.port_id, sizeof(event->nl.port_id), &(sock->nl_pid));
        bpf_probe_read(&event->nl.groups, sizeof(event->nl.groups), &(sock->nl_groups));
    }
    #endif

    if (e_type == DIO_ACCEPT4) {
        bpf_probe_read(&event->flags, sizeof(event->flags), (int*)&args->args[5]);
    }

    handle_calls_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));
    if (events.perf_submit(ctx, event, sizeof(*event)) == 0) handle_submitted_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));
    else handle_lost_event(key->type, event->base_fd_info.base.return_time, sizeof(*event));

    return 0;
}
int exit_function_connect_accept(struct bpf_raw_tracepoint_args *ctx) {
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
    #ifdef ONE2MANY
    struct event_t *event = percpu_array_events_global.lookup((int*)&key.type);
    if (!event) return 1;
    return submit_network_accept_event((struct pt_regs *) ctx, &event->event_network_accept, key.type, &proc_info, &key, return_value);
    #endif
    #ifndef ONE2MANY
    int event_key = 0;
    EventAccept *event = percpu_array_events_network_accept.lookup(&event_key);
    if (!event) return 1;
    return submit_network_accept_event((struct pt_regs *) ctx, event, key.type, &proc_info, &key, return_value);
    #endif
}

int exit_function_sockopt(struct bpf_raw_tracepoint_args *ctx) {
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

    EventSockopt event = {};
    if (createEventBase(&event.base_fd_info.base, key.type, &proc_info, &(args->timestamp), &return_value)) return 1;
    bpf_probe_read(&event.base_fd_info.file_fd.file_descriptor, sizeof(event.base_fd_info.file_fd), (int*)&args->args[2]);

    if (check_inode((struct pt_regs *) ctx, key.type, &event.base_fd_info.file_fd, &event.base_fd_info.base.call_time) > 1) return 1;

    bpf_probe_read(&event.level, sizeof(event.level), (int*)&args->args[3]);
    bpf_probe_read(&event.optname, sizeof(event.optname), (int*)&args->args[4]);

    handle_calls_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    if (events.perf_submit((struct pt_regs *) ctx, &event, sizeof(event)) == 0) handle_submitted_event(key.type, event.base_fd_info.base.return_time, sizeof(event));
    else handle_lost_event(key.type, event.base_fd_info.base.return_time, sizeof(event));

    return 0;
}

static int getBufFromBuf(EventNetworkData *event, struct entry_args_t *args, long offset) {
    struct sockaddr *sock;
    bpf_probe_read(&sock, sizeof(sock), (struct sockaddr *) &args->args[6]);

    u32 addr_len;
    if (event->event_network.base_fd_info.base.etype == DIO_RECVFROM)
        bpf_probe_read(&addr_len, sizeof(addr_len), (u32 *) args->args[7]);
    else
        bpf_probe_read(&addr_len, sizeof(addr_len), (u32 *) &args->args[7]);

    if (getNetworkDataFromSock(sock, addr_len, &event->event_network)) return 1;

    uint64_t len;
    bpf_probe_read(&len, sizeof(len), &args->args[4]);
    if (len > event->event_network.base_fd_info.base.return_value) len = event->event_network.base_fd_info.base.return_value;
    if (len < 0) len = 0;
    if (len >= MAX_BUF_SIZE) len = MAX_BUF_SIZE;


    bpf_probe_read(&event->data.bytes_request, sizeof(event->data.bytes_request), &args->args[4]);
    event->data.offset = offset;
    char *buf;
    bpf_probe_read(&buf, sizeof(buf), &args->args[3]);

    #if COMPUTE_HASH==1
    event->data.captured_size = len;
    if (copyDataToPerCpu(&event->data, buf, event->data.captured_size)) return 1;
    #endif

    #if COMPUTE_HASH==2
    event->data.captured_size = len;
    event->data.hash = xxhash32(buf, event->data.captured_size, 12345);
    #endif

    return 0;
}
static int submit_network_data_event_sendto_recvfrom(struct pt_regs *ctx, EventNetworkData *event, enum event_type e_type, struct pid_info_t *proc_info, struct event_key_t *key, long return_value)
{
    struct data_args_t *buffer_data = entry_data_buf_args.lookup(key);
    if (!buffer_data) return 1;
    entry_data_buf_args.delete(key);

    if (createEventBase(&event->event_network.base_fd_info.base, e_type, proc_info, &(buffer_data->args.timestamp), &return_value)) return 1;
    event->event_network.base_fd_info.file_fd = buffer_data->file_fd;

    getBufFromBuf(event, &buffer_data->args, buffer_data->offset);
    bpf_probe_read(&event->flags, sizeof(event->flags), &buffer_data->args.args[5]);
    get_socket_info(event->event_network.base_fd_info.file_fd.file_descriptor, &event->sock_data);

    handle_calls_event(key->type, event->event_network.base_fd_info.base.return_time, sizeof(*event));
    if (events.perf_submit(ctx, event, sizeof(*event)) == 0) handle_submitted_event(key->type, event->event_network.base_fd_info.base.return_time, sizeof(*event));
    else handle_lost_event(key->type, event->event_network.base_fd_info.base.return_time, sizeof(*event));

    return 0;
}
int exit_function_sendto_recvfrom(struct bpf_raw_tracepoint_args *ctx)
{
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

    #ifdef ONE2MANY
    struct event_t *event = percpu_array_events_global.lookup((int*)&key.type);
    if (!event) return 1;
    return submit_network_data_event_sendto_recvfrom((struct pt_regs *) ctx, &event->event_network_data, key.type, &proc_info, &key, return_value);
    #endif

    #ifndef ONE2MANY
    int event_key = 0;
    EventNetworkData *event = percpu_array_events_network_data.lookup(&event_key);
    if (!event) return 1;
    return submit_network_data_event_sendto_recvfrom((struct pt_regs *) ctx, event, key.type, &proc_info, &key, return_value);
    #endif

    return 0;
}

static int getBufFromIov(EventNetworkData *event, void *msg_arg, long offset) {
    struct user_msghdr *msg = NULL;
    bpf_probe_read(&msg, sizeof(msg), (struct user_msghdr *) msg_arg);
    struct sockaddr *sock = NULL;
    bpf_probe_read(&sock, sizeof(sock), (struct sockaddr *) &msg->msg_name);

    int addr_len;
    bpf_probe_read(&addr_len, sizeof(addr_len), (int *) &msg->msg_namelen);

    if (getNetworkDataFromSock(sock, addr_len, &event->event_network)) return 1;

    const struct iovec *vec;
    bpf_probe_read(&vec, sizeof(vec), &msg->msg_iov);

    size_t vlen;
    bpf_probe_read(&vlen, sizeof(vlen), &msg->msg_iovlen);

    size_t total_bytes_request = 0;
    if (vlen > 1) {
        int i = 0;
        size_t bytes_request = 0;
        while (i < vlen) {
            if (i > 1024) break;
            bpf_probe_read(&bytes_request, sizeof(bytes_request), &vec[i].iov_len);
            total_bytes_request += bytes_request;
            i++;
        }
    } else {
        bpf_probe_read(&total_bytes_request, sizeof(total_bytes_request), &vec->iov_len);
    }

    uint64_t len;
    bpf_probe_read(&len, sizeof(len), &vec[0].iov_len);
    if (len > event->event_network.base_fd_info.base.return_value) len = event->event_network.base_fd_info.base.return_value;
    if (len < 0) len = 0;
    if (len >= MAX_BUF_SIZE) len = MAX_BUF_SIZE;


    event->data.bytes_request = total_bytes_request;
    event->data.offset = offset;
    char *buf;
    bpf_probe_read(&buf, sizeof(buf), &vec[0].iov_base);

    #if COMPUTE_HASH==1
    event->data.captured_size = len;
    if (copyDataToPerCpu(&event->data, buf, event->data.captured_size)) return 1;
    #endif

    #if COMPUTE_HASH==2
    event->data.captured_size = len;
    event->data.hash = xxhash32(buf, event->data.captured_size, 12345);
    #endif

    return 0;
}
static int submit_network_data_event(struct pt_regs *ctx, EventNetworkData *event, enum event_type e_type, struct pid_info_t *proc_info, struct event_key_t *key, long return_value)
{
    struct data_args_t *buffer_data = entry_data_buf_args.lookup(key);
    if (!buffer_data) return 1;
    entry_data_buf_args.delete(key);

    if (createEventBase(&event->event_network.base_fd_info.base, e_type, proc_info, &(buffer_data->args.timestamp), &return_value)) return 1;
    event->event_network.base_fd_info.file_fd = buffer_data->file_fd;

    getBufFromIov(event, &buffer_data->args.args[3], buffer_data->offset);
    bpf_probe_read(&event->flags, sizeof(event->flags), &buffer_data->args.args[4]);
    get_socket_info(event->event_network.base_fd_info.file_fd.file_descriptor, &event->sock_data);

    handle_calls_event(key->type, event->event_network.base_fd_info.base.return_time, sizeof(*event));
    if (events.perf_submit(ctx, event, sizeof(*event)) == 0) handle_submitted_event(key->type, event->event_network.base_fd_info.base.return_time, sizeof(*event));
    else handle_lost_event(key->type, event->event_network.base_fd_info.base.return_time, sizeof(*event));

    return 0;
}
int exit_function_send_recv_msg(struct bpf_raw_tracepoint_args *ctx)
{
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

    #ifdef ONE2MANY
    struct event_t *event = percpu_array_events_global.lookup((int*)&key.type);
    if (!event) return 1;
    return submit_network_data_event((struct pt_regs *) ctx, &event->event_network_data, key.type, &proc_info, &key, return_value);
    #endif

    #ifndef ONE2MANY
    int event_key = 0;
    EventNetworkData *event = percpu_array_events_network_data.lookup(&event_key);
    if (!event) return 1;
    return submit_network_data_event((struct pt_regs *) ctx, event, key.type, &proc_info, &key, return_value);
    #endif

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
    EventBase context = {};
	context.etype = DIO_PROCESS_FORK;
    context.kpid = proc_info.kpid;
    context.tgid = proc_info.tgid;
    context.ppid = proc_info.ppid;
    context.call_time = bpf_ktime_get_ns();
    bpf_probe_read(&context.comm, sizeof(context.comm), &proc_info.comm);

    EventProcess event = {};
    event.context = context;
    event.child_pid = child_kpid;

    handle_calls_event(DIO_PROCESS_FORK, event.context.call_time, sizeof(event));
    if (events.perf_submit((struct pt_regs*)args, &event, sizeof(event)) == 0) handle_submitted_event(DIO_PROCESS_FORK, event.context.call_time, sizeof(event));
    else handle_lost_event(DIO_PROCESS_FORK, event.context.call_time, sizeof(event));
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
    EventBase context = {};
	context.etype = DIO_PROCESS_END;
    context.kpid = proc_info.kpid;
    context.tgid = proc_info.tgid;
    context.ppid = proc_info.ppid;
    context.call_time = bpf_ktime_get_ns();
    context.return_value = task->exit_code >> 8;
    bpf_probe_read(&context.comm, sizeof(context.comm), &proc_info.comm);

    EventProcess event = {};
    event.context = context;
    event.child_pid = args->pid;

    handle_calls_event(DIO_PROCESS_END, event.context.call_time, sizeof(event));
    if (events.perf_submit((struct pt_regs*)args, &event, sizeof(event)) == 0) handle_submitted_event(DIO_PROCESS_END, event.context.call_time, sizeof(event));
    else handle_lost_event(DIO_PROCESS_END, event.context.call_time, sizeof(event));

    return 0;
}

// ----------------------------------

/**
 * @brief eBPF program for "destroy_inode" events (entry point)
 *
 * @param ctx
 * @param inode
 * @return int
 */
int entry__destroy_inode(struct pt_regs *ctx, struct inode *inode) {
    FileFDKey file_tag = {};

    if (get_file_tag(&file_tag, inode)) return 1;

    u64 *i_timestamp = opened_fds.lookup(&file_tag);
    if (i_timestamp != NULL) {
        opened_fds.delete(&file_tag);
    }

    return 0;
}
package events

type Probe struct {
	ProbeType    string
	ProbeName    string
	FunctionName string
}

type Arg struct {
	Name string
	Type string
}
type EventInfo struct {
	ID        uint32
	EventType string
	Name      string
	Probes    []Probe
	Enable    bool
	Category  string
	FuncType  string
	Args      []Arg
}

const (
	DIO_PATH_EVENT uint32 = iota + 1
	DIO_PROCESS_FORK
	DIO_PROCESS_END
	DIO_DESTROY_INODE
	DIO_CREAT
	DIO_OPEN
	DIO_OPENAT
	DIO_READ
	DIO_PREAD64
	DIO_READV
	DIO_WRITE
	DIO_PWRITE64
	DIO_WRITEV
	DIO_CLOSE
	DIO_TRUNCATE
	DIO_FTRUNCATE
	DIO_RENAME
	DIO_RENAMEAT
	DIO_RENAMEAT2
	DIO_UNLINK
	DIO_UNLINKAT
	DIO_FSYNC
	DIO_FDATASYNC
	DIO_READAHEAD
	DIO_READLINK
	DIO_READLINKAT
	DIO_STAT
	DIO_LSTAT
	DIO_FSTAT
	DIO_FSTATAT
	DIO_FSTATFS
	DIO_GETXATTR
	DIO_LGETXATTR
	DIO_FGETXATTR
	DIO_SETXATTR
	DIO_LSETXATTR
	DIO_FSETXATTR
	DIO_REMOVEXATTR
	DIO_LREMOVEXATTR
	DIO_FREMOVEXATTR
	DIO_LISTXATTR
	DIO_LLISTXATTR
	DIO_FLISTXATTR
	DIO_LISTEN
	DIO_BIND
	DIO_ACCEPT
	DIO_ACCEPT4
	DIO_CONNECT
	DIO_RECVFROM
	DIO_RECVMSG
	DIO_SENDTO
	DIO_SENDMSG
	DIO_SOCKET
	DIO_SOCKETPAIR
	DIO_GETSOCKOPT
	DIO_SETSOCKOPT
	DIO_MKNOD
	DIO_MKNODAT
	DIO_LSEEK
)

var SuportedEvents = map[uint32]EventInfo{
	DIO_PROCESS_FORK:  {ID: DIO_PROCESS_FORK, Name: "process_fork", EventType: "process", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "sched:sched_process_fork", FunctionName: "on_fork"}}, Enable: true, FuncType: "", Category: "Process", Args: []Arg{{Name: "child_pid", Type: "uint32"}}},
	DIO_PROCESS_END:   {ID: DIO_PROCESS_END, Name: "process_end", EventType: "process", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "sched:sched_process_exit", FunctionName: "on_exit"}}, Enable: true, FuncType: "", Category: "Process", Args: []Arg{{Name: "pid", Type: "uint32"}}},
	DIO_PATH_EVENT:    {ID: DIO_PATH_EVENT, Name: "event_path", EventType: "path", Enable: false, FuncType: "", Category: ""},
	DIO_DESTROY_INODE: {ID: DIO_DESTROY_INODE, Name: "destroy_inode", EventType: "storage", Probes: []Probe{{ProbeType: "kprobe", ProbeName: "destroy_inode", FunctionName: "entry__destroy_inode"}}, Enable: true, FuncType: "", Category: "Process"},

	// data events
	DIO_READ:      {ID: DIO_READ, Name: "read", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_read", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_read", FunctionName: "exit_function_storage_data"}}, Enable: false, FuncType: "data", Category: "FileManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "buf", Type: "pointer"}, {Name: "count", Type: "uint64"}}},
	DIO_PREAD64:   {ID: DIO_PREAD64, Name: "pread64", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_pread64", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_pread64", FunctionName: "exit_function_storage_data"}}, Enable: false, FuncType: "data", Category: "FileManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "buf", Type: "pointer"}, {Name: "count", Type: "uint64"}, {Name: "offset", Type: "int64"}}},
	DIO_READV:     {ID: DIO_READV, Name: "readv", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_readv", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_readv", FunctionName: "exit_function_storage_iov_data"}}, Enable: false, FuncType: "data", Category: "FileManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "iov", Type: "pointer"}, {Name: "iovcnt", Type: "uint64"}}},
	DIO_WRITE:     {ID: DIO_WRITE, Name: "write", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_write", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_write", FunctionName: "exit_function_storage_data"}}, Enable: false, FuncType: "data", Category: "FileManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "buf", Type: "pointer"}, {Name: "count", Type: "uint64"}}},
	DIO_PWRITE64:  {ID: DIO_PWRITE64, Name: "pwrite64", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_pwrite64", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_pwrite64", FunctionName: "exit_function_storage_data"}}, Enable: false, FuncType: "data", Category: "FileManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "buf", Type: "pointer"}, {Name: "count", Type: "uint64"}, {Name: "offset", Type: "int64"}}},
	DIO_WRITEV:    {ID: DIO_WRITEV, Name: "writev", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_writev", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_writev", FunctionName: "exit_function_storage_iov_data"}}, Enable: false, FuncType: "data", Category: "FileManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "iov", Type: "pointer"}, {Name: "iovcnt", Type: "uint64"}}},
	DIO_FSYNC:     {ID: DIO_FSYNC, Name: "fsync", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_fsync", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_fsync", FunctionName: "exit_function_fd"}}, Enable: false, FuncType: "data", Category: "DeviceManagement", Args: []Arg{{Name: "fd", Type: "uint32"}}},
	DIO_FDATASYNC: {ID: DIO_FDATASYNC, Name: "fdatasync", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_fdatasync", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_fdatasync", FunctionName: "exit_function_fd"}}, Enable: false, FuncType: "data", Category: "DeviceManagement", Args: []Arg{{Name: "fd", Type: "uint32"}}},
	DIO_READAHEAD: {ID: DIO_READAHEAD, Name: "readahead", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_readahead", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_readahead", FunctionName: "exit_function_readahead"}}, Enable: false, FuncType: "data", Category: "DeviceManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "offset", Type: "int64"}, {Name: "count", Type: "uint64"}}},

	// metadata events
	DIO_CREAT:      {ID: DIO_CREAT, Name: "creat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_creat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_creat", FunctionName: "exit_function_open"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "mode", Type: "uint32"}}},
	DIO_OPEN:       {ID: DIO_OPEN, Name: "open", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_open", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_open", FunctionName: "exit_function_open"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "flags", Type: "uint32"}, {Name: "mode", Type: "uint32"}}},
	DIO_OPENAT:     {ID: DIO_OPENAT, Name: "openat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_openat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_openat", FunctionName: "exit_function_open"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "dirfd", Type: "uint32"}, {Name: "pathname", Type: "pointer"}, {Name: "flags", Type: "uint32"}, {Name: "mode", Type: "uint32"}}},
	DIO_CLOSE:      {ID: DIO_CLOSE, Name: "close", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_close", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_close", FunctionName: "exit_function_close"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "fd", Type: "uint32"}}},
	DIO_LSEEK:      {ID: DIO_LSEEK, Name: "lseek", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_lseek", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_lseek", FunctionName: "exit_function_lseek"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "offset", Type: "int64"}, {Name: "whence", Type: "uint32"}}},
	DIO_TRUNCATE:   {ID: DIO_TRUNCATE, Name: "truncate", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_truncate", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_truncate", FunctionName: "exit_function_truncate"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "length", Type: "uint64"}}},
	DIO_FTRUNCATE:  {ID: DIO_FTRUNCATE, Name: "ftruncate", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_ftruncate", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_ftruncate", FunctionName: "exit_function_ftruncate"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "length", Type: "uint64"}}},
	DIO_RENAME:     {ID: DIO_RENAME, Name: "rename", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_rename", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_rename", FunctionName: "exit_function_rename"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "oldpath", Type: "pointer"}, {Name: "newpath", Type: "pointer"}}},
	DIO_RENAMEAT:   {ID: DIO_RENAME, Name: "renameat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_renameat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_renameat", FunctionName: "exit_function_rename"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "olddirfd", Type: "uint32"}, {Name: "oldpath", Type: "pointer"}, {Name: "newdirfd", Type: "uint32"}, {Name: "newpath", Type: "pointer"}}},
	DIO_RENAMEAT2:  {ID: DIO_RENAME, Name: "renameat2", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_renameat2", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_renameat2", FunctionName: "exit_function_rename"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "olddirfd", Type: "uint32"}, {Name: "oldpath", Type: "pointer"}, {Name: "newdirfd", Type: "uint32"}, {Name: "newpath", Type: "pointer"}, {Name: "flags", Type: "uint32"}}},
	DIO_UNLINK:     {ID: DIO_UNLINK, Name: "unlink", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_unlink", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_unlink", FunctionName: "exit_function_base_path"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}}},
	DIO_UNLINKAT:   {ID: DIO_UNLINKAT, Name: "unlinkat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_unlinkat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_unlinkat", FunctionName: "exit_function_base_path"}}, Enable: false, FuncType: "metadata", Category: "FileManagement", Args: []Arg{{Name: "dirfd", Type: "uint32"}, {Name: "pathname", Type: "pointer"}, {Name: "flags", Type: "uint32"}}},
	DIO_READLINK:   {ID: DIO_READLINK, Name: "readlink", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_readlink", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_readlink", FunctionName: "exit_function_readlink"}}, Enable: false, FuncType: "metadata", Category: "Other", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "buf", Type: "pointer"}, {Name: "bufsiz", Type: "int32"}}},
	DIO_READLINKAT: {ID: DIO_READLINKAT, Name: "readlinkat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_readlinkat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_readlinkat", FunctionName: "exit_function_readlink"}}, Enable: false, FuncType: "metadata", Category: "Other", Args: []Arg{{Name: "dirfd", Type: "uint32"}, {Name: "pathname", Type: "pointer"}, {Name: "buf", Type: "pointer"}, {Name: "bufsiz", Type: "uint64"}}},
	DIO_STAT:       {ID: DIO_STAT, Name: "stat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_newstat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_newstat", FunctionName: "exit_function_base_path"}}, Enable: false, FuncType: "metadata", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "statbuf", Type: "pointer"}}},
	DIO_FSTAT:      {ID: DIO_FSTAT, Name: "fstat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_newfstat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_newfstat", FunctionName: "exit_function_fd"}}, Enable: false, FuncType: "metadata", Category: "InformationManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "statbuf", Type: "pointer"}}},
	DIO_LSTAT:      {ID: DIO_LSTAT, Name: "lstat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_newlstat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_newlstat", FunctionName: "exit_function_base_path"}}, Enable: false, FuncType: "metadata", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "statbuf", Type: "pointer"}}},
	DIO_FSTATFS:    {ID: DIO_FSTATFS, Name: "fstatfs", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_fstatfs", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_fstatfs", FunctionName: "exit_function_fd"}}, Enable: false, FuncType: "metadata", Category: "InformationManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "statbuf", Type: "pointer"}}},
	DIO_FSTATAT:    {ID: DIO_FSTATAT, Name: "fstatat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_newfstatat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_newfstatat", FunctionName: "exit_function_base_path"}}, Enable: false, FuncType: "metadata", Category: "InformationManagement", Args: []Arg{{Name: "dirfd", Type: "uint32"}, {Name: "pathname", Type: "pointer"}, {Name: "statbuf", Type: "pointer"}, {Name: "flags", Type: "uint32"}}},

	// extended attributes events
	DIO_SETXATTR:     {ID: DIO_SETXATTR, Name: "setxattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_setxattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_setxattr", FunctionName: "exit_function_xattr"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "name", Type: "pointer"}, {Name: "value", Type: "pointer"}, {Name: "size", Type: "uint64"}, {Name: "flags", Type: "uint32"}}},
	DIO_LSETXATTR:    {ID: DIO_LSETXATTR, Name: "lsetxattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_lsetxattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_lsetxattr", FunctionName: "exit_function_xattr"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "name", Type: "pointer"}, {Name: "value", Type: "pointer"}, {Name: "size", Type: "uint64"}, {Name: "flags", Type: "uint32"}}},
	DIO_FSETXATTR:    {ID: DIO_FSETXATTR, Name: "fsetxattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_fsetxattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_fsetxattr", FunctionName: "exit_function_xattr_fd"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "name", Type: "pointer"}, {Name: "value", Type: "pointer"}, {Name: "size", Type: "uint64"}, {Name: "flags", Type: "uint32"}}},
	DIO_GETXATTR:     {ID: DIO_GETXATTR, Name: "getxattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_getxattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_getxattr", FunctionName: "exit_function_xattr"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "name", Type: "pointer"}, {Name: "value", Type: "pointer"}, {Name: "size", Type: "uint64"}}},
	DIO_LGETXATTR:    {ID: DIO_LGETXATTR, Name: "lgetxattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_lgetxattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_lgetxattr", FunctionName: "exit_function_xattr"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "name", Type: "pointer"}, {Name: "value", Type: "pointer"}, {Name: "size", Type: "uint64"}}},
	DIO_FGETXATTR:    {ID: DIO_FGETXATTR, Name: "fgetxattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_fgetxattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_fgetxattr", FunctionName: "exit_function_xattr_fd"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "name", Type: "pointer"}, {Name: "value", Type: "pointer"}, {Name: "size", Type: "uint64"}}},
	DIO_LISTXATTR:    {ID: DIO_LISTXATTR, Name: "listxattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_listxattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_listxattr", FunctionName: "exit_function_base_path"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "list", Type: "pointer"}, {Name: "size", Type: "uint64"}}},
	DIO_LLISTXATTR:   {ID: DIO_LLISTXATTR, Name: "llistxattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_llistxattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_llistxattr", FunctionName: "exit_function_base_path"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "list", Type: "pointer"}, {Name: "size", Type: "uint64"}}},
	DIO_FLISTXATTR:   {ID: DIO_FLISTXATTR, Name: "flistxattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_flistxattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_flistxattr", FunctionName: "exit_function_fd"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "list", Type: "pointer"}, {Name: "size", Type: "uint64"}}},
	DIO_REMOVEXATTR:  {ID: DIO_REMOVEXATTR, Name: "removexattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_removexattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_removexattr", FunctionName: "exit_function_xattr"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "name", Type: "pointer"}}},
	DIO_LREMOVEXATTR: {ID: DIO_LREMOVEXATTR, Name: "lremovexattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_lremovexattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_lremovexattr", FunctionName: "exit_function_xattr"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "name", Type: "pointer"}}},
	DIO_FREMOVEXATTR: {ID: DIO_FREMOVEXATTR, Name: "fremovexattr", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_fremovexattr", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_fremovexattr", FunctionName: "exit_function_xattr_fd"}}, Enable: false, FuncType: "extended attributes", Category: "InformationManagement", Args: []Arg{{Name: "fd", Type: "uint32"}, {Name: "name", Type: "pointer"}}},

	// directory management events
	DIO_MKNOD:   {ID: DIO_MKNOD, Name: "mknod", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_mknod", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_mknod", FunctionName: "exit_function_mknod"}}, Enable: false, FuncType: "directory management", Category: "Other", Args: []Arg{{Name: "pathname", Type: "pointer"}, {Name: "mode", Type: "uint32"}, {Name: "dev", Type: "uint64"}}},
	DIO_MKNODAT: {ID: DIO_MKNODAT, Name: "mknodat", EventType: "storage", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_mknodat", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_mknodat", FunctionName: "exit_function_mknod"}}, Enable: false, FuncType: "directory management", Category: "Other", Args: []Arg{{Name: "dirfd", Type: "uint32"}, {Name: "pathname", Type: "pointer"}, {Name: "mode", Type: "uint32"}, {Name: "dev", Type: "uint16"}}},

	// network events
	DIO_SOCKET:     {ID: DIO_SOCKET, Name: "socket", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_socket", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_socket", FunctionName: "exit_function_socket"}}, Enable: false, FuncType: "metadata", Category: "Communication", Args: []Arg{{Name: "domain", Type: "int32"}, {Name: "type", Type: "int32"}, {Name: "protocol", Type: "int32"}}},
	DIO_SOCKETPAIR: {ID: DIO_SOCKETPAIR, Name: "socketpair", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_socketpair", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_socketpair", FunctionName: "exit_function_socket"}}, Enable: false, FuncType: "metadata", Category: "Communication", Args: []Arg{{Name: "domain", Type: "int32"}, {Name: "type", Type: "int32"}, {Name: "protocol", Type: "int32"}, {Name: "sv", Type: "pointer"}}},
	DIO_LISTEN:     {ID: DIO_LISTEN, Name: "listen", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_listen", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_listen", FunctionName: "exit_function_listen"}}, Enable: false, FuncType: "metadata", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "backlog", Type: "int32"}}},
	DIO_CONNECT:    {ID: DIO_CONNECT, Name: "connect", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_connect", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_connect", FunctionName: "exit_function_connect_accept"}}, Enable: false, FuncType: "metadata", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "addr", Type: "pointer"}, {Name: "addrlen", Type: "uint32"}}},
	DIO_ACCEPT:     {ID: DIO_ACCEPT, Name: "accept", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_accept", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_accept", FunctionName: "exit_function_connect_accept"}}, Enable: false, FuncType: "metadata", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "addr", Type: "pointer"}, {Name: "addrlen", Type: "pointer"}}},
	DIO_ACCEPT4:    {ID: DIO_ACCEPT4, Name: "accept4", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_accept4", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_accept4", FunctionName: "exit_function_connect_accept"}}, Enable: false, FuncType: "metadata", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "addr", Type: "pointer"}, {Name: "addrlen", Type: "pointer"}, {Name: "flags", Type: "uint32"}}},
	DIO_BIND:       {ID: DIO_BIND, Name: "bind", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_bind", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_bind", FunctionName: "exit_function_bind"}}, Enable: false, FuncType: "metadata", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "addr", Type: "pointer"}, {Name: "addrlen", Type: "uint32"}}},
	DIO_SENDTO:     {ID: DIO_SENDTO, Name: "sendto", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_sendto", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_sendto", FunctionName: "exit_function_sendto_recvfrom"}}, Enable: false, FuncType: "data", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "buf", Type: "pointer"}, {Name: "len", Type: "uint64"}, {Name: "flags", Type: "uint32"}, {Name: "dest_addr", Type: "pointer"}, {Name: "addrlen", Type: "uint32"}}},
	DIO_SENDMSG:    {ID: DIO_SENDMSG, Name: "sendmsg", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_sendmsg", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_sendmsg", FunctionName: "exit_function_send_recv_msg"}}, Enable: false, FuncType: "data", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "msg", Type: "pointer"}, {Name: "flags", Type: "uint32"}}},
	DIO_RECVFROM:   {ID: DIO_RECVFROM, Name: "recvfrom", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_recvfrom", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_recvfrom", FunctionName: "exit_function_sendto_recvfrom"}}, Enable: false, FuncType: "data", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "buf", Type: "pointer"}, {Name: "len", Type: "uint64"}, {Name: "flags", Type: "uint32"}, {Name: "src_addr", Type: "pointer"}, {Name: "addrlen", Type: "pointer"}}},
	DIO_RECVMSG:    {ID: DIO_RECVMSG, Name: "recvmsg", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_recvmsg", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_recvmsg", FunctionName: "exit_function_send_recv_msg"}}, Enable: false, FuncType: "data", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "msg", Type: "pointer"}, {Name: "flags", Type: "uint32"}}},
	DIO_SETSOCKOPT: {ID: DIO_SETSOCKOPT, Name: "setsockopt", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_setsockopt", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_setsockopt", FunctionName: "exit_function_sockopt"}}, Enable: false, FuncType: "metadata", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "level", Type: "int32"}, {Name: "optname", Type: "int32"}, {Name: "optval", Type: "pointer"}, {Name: "optlen", Type: "uint32"}}},
	DIO_GETSOCKOPT: {ID: DIO_GETSOCKOPT, Name: "getsockopt", EventType: "network", Probes: []Probe{{ProbeType: "tracepoint", ProbeName: "syscalls:sys_enter_getsockopt", FunctionName: "enter_function"}, {ProbeType: "tracepoint", ProbeName: "syscalls:sys_exit_getsockopt", FunctionName: "exit_function_sockopt"}}, Enable: false, FuncType: "metadata", Category: "Communication", Args: []Arg{{Name: "sockfd", Type: "uint32"}, {Name: "level", Type: "int32"}, {Name: "optname", Type: "int32"}, {Name: "optval", Type: "pointer"}, {Name: "optlen", Type: "pointer"}}},
}

var EventName2ID = map[string]uint32{
	"process_fork": DIO_PROCESS_FORK,
	"process_end":  DIO_PROCESS_END,
	"event_path":   DIO_PATH_EVENT,
	"read":         DIO_READ,
	"pread64":      DIO_PREAD64,
	"readv":        DIO_READV,
	"write":        DIO_WRITE,
	"pwrite64":     DIO_PWRITE64,
	"writev":       DIO_WRITEV,
	"creat":        DIO_CREAT,
	"open":         DIO_OPEN,
	"openat":       DIO_OPENAT,
	"close":        DIO_CLOSE,
	"truncate":     DIO_TRUNCATE,
	"ftruncate":    DIO_FTRUNCATE,
	"rename":       DIO_RENAME,
	"renameat":     DIO_RENAMEAT,
	"renameat2":    DIO_RENAMEAT2,
	"unlink":       DIO_UNLINK,
	"unlinkat":     DIO_UNLINKAT,
	"fsync":        DIO_FSYNC,
	"fdatasync":    DIO_FDATASYNC,
	"readahead":    DIO_READAHEAD,
	"readlink":     DIO_READLINK,
	"readlinkat":   DIO_READLINKAT,
	"stat":         DIO_STAT,
	"lstat":        DIO_LSTAT,
	"fstat":        DIO_FSTAT,
	"fstatat":      DIO_FSTATAT,
	"fstatfs":      DIO_FSTATFS,
	"getxattr":     DIO_GETXATTR,
	"lgetxattr":    DIO_LGETXATTR,
	"fgetxattr":    DIO_FGETXATTR,
	"setxattr":     DIO_SETXATTR,
	"lsetxattr":    DIO_LSETXATTR,
	"fsetxattr":    DIO_FSETXATTR,
	"removexattr":  DIO_REMOVEXATTR,
	"lremovexattr": DIO_LREMOVEXATTR,
	"fremovexattr": DIO_FREMOVEXATTR,
	"listxattr":    DIO_LISTXATTR,
	"llistxattr":   DIO_LLISTXATTR,
	"flistxattr":   DIO_FLISTXATTR,
	"listen":       DIO_LISTEN,
	"bind":         DIO_BIND,
	"accept":       DIO_ACCEPT,
	"accept4":      DIO_ACCEPT4,
	"connect":      DIO_CONNECT,
	"recvfrom":     DIO_RECVFROM,
	"recvmsg":      DIO_RECVMSG,
	"sendto":       DIO_SENDTO,
	"sendmsg":      DIO_SENDMSG,
	"socket":       DIO_SOCKET,
	"socketpair":   DIO_SOCKETPAIR,
	"getsockopt":   DIO_GETSOCKOPT,
	"setsockopt":   DIO_SETSOCKOPT,
	"mknod":        DIO_MKNOD,
	"mknodat":      DIO_MKNODAT,
	"lseek":        DIO_LSEEK,
}

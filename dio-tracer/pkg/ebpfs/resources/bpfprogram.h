#define TASK_COMM_LEN		16
#define MY_UNIX_PATH_MAX	100
#define FILENAME_MAX		1024
#define MAX_JUMPS 			//MAX_JUMPS//
#define MAX_FILE_OFFSET		(FILENAME_MAX>>1)
#define TARGET_PATH_LEN		64
#define SUB_STR_MAX			512
#define MAX_BUF_SIZE 		4096
#define PER_CPU_ENTRIES 	65536
#define MAX_ATTR_NAME_SIZE  32
#define NARGS				6
#define TRACER_COMM			"dio"
#define TARGET_PATHS_LEN 	//TARGET_PATHS_LEN//
#define COMM_FILTER 		//COMM_FILTER//
#define TARGET_COMM 		//TARGET_COMM//
#define PID_FILTER 			//PID_FILTER//
#define TID_FILTER 			//TID_FILTER//
#define CHILDS_FILTER		//CHILDS_FILTER//
#define DISCARD_ERRORS		//DISCARD_ERRORS//
#define TOTAL_EVENTS		//TOTAL_EVENTS//
#define DISCARD_DIRECTORIES //DISCARD_DIRECTORIES//
//TRACE_STDIN//
//TRACE_STDOUT//
//TRACE_STDERR//
//TRACE_SOCKADDR//
//TRACE_SOCKDATA//
//COMPUTE_HASH//
//FILTER_FILES//
//ONE2MANY//
//CAPTURE_ARG_PATHS//
//PROFILE_ON//
//CAPTURE_PROC_EVENTS//

enum event_type {
	UNUSED = 0,
	//ENUM_TYPE//
};

typedef struct file_fd_key_t {
	uint32_t dev;
    uint32_t ino;
} __attribute__((packed)) FileFDKey;

typedef struct message_content_t {
	int n_ref;
	char msg[MAX_BUF_SIZE];
	int _padding;
} Message_Content;

typedef struct stats_info_t {
	uint32_t n_entries;
	uint32_t n_exits;
	uint32_t n_errors;
	uint32_t n_lost;
} __attribute__((packed)) Stats_Info;

typedef struct file_info_t {
    uint32_t n_ref;
	uint16_t file_type;
	uint32_t offset;
	uint32_t size;
	char filename[FILENAME_MAX];
} FileInfo;

typedef struct target_path_entry_t {
    int size;
	char name[TARGET_PATH_LEN];
} TargetPathEntry;

// ------

typedef struct _inet_addr_t {
	uint64_t addr[2];
	uint16_t port;
} __attribute__((packed)) Inet_Addr;

typedef struct _unix_addr_t {
	uint8_t path[MY_UNIX_PATH_MAX];
} __attribute__((packed)) Unix_Addr;

typedef struct _netlink_addr_t {
	uint32_t port_id;
    uint32_t groups;
} __attribute__((packed)) Netlink_Addr;

typedef struct socket_data_t {
	#ifdef TRACE_SOCKDATA
	uint16_t family;
	uint64_t saddr[2];
	uint16_t sport;
	uint64_t daddr[2];
	uint16_t dport;
	#endif
} __attribute__((packed)) SocketData;

// ------

typedef struct event_raw_t {
	enum event_type etype;
	uint32_t kpid;
	uint32_t tgid;
	uint32_t ppid;
    uint64_t call_time;
	uint64_t return_time;
	int64_t return_value;
    char comm[TASK_COMM_LEN];
	uint16_t cpu;
    uint64_t args[NARGS];
} __attribute__((packed)) Event;

typedef struct event_base_t {
	enum event_type etype;
	uint32_t kpid;
	uint32_t tgid;
	uint32_t ppid;
    uint64_t call_time;
	uint64_t return_time;
	int64_t return_value;
    char comm[TASK_COMM_LEN];
	uint16_t cpu;
} __attribute__((packed)) EventBase;

typedef struct event_process_t {
	EventBase context;
	uint32_t child_pid;
} __attribute__((packed)) EventProcess;

typedef struct event_path_t
{
	enum event_type etype;
	FileFDKey f_tag;
	uint64_t timestamp;
	int index;
	int n_ref;
	uint16_t cpu;
} __attribute__((packed)) EventPath;

typedef struct fd_info_t {
	int32_t file_descriptor;
	FileFDKey f_tag;
	uint64_t timestamp;
} __attribute__((packed)) FDInfo;

typedef struct event_base_fd_t {
	EventBase base;
	FDInfo file_fd;
} __attribute__((packed)) EventBaseFD;

typedef struct event_open_t {
	EventBaseFD base_fd_info;
	int flags;
	uint16_t mode;
} __attribute__((packed)) EventOpen;

typedef struct event2paths {
	EventBase base;
	#ifdef CAPTURE_ARG_PATHS
	int index;
	uint32_t n_ref;
	uint16_t cpu;
	uint32_t oldname_len;
	uint32_t newname_len;
	#endif
	unsigned int flags;
} __attribute__((packed)) Event2Paths;

typedef struct event_truncate_t {
	EventBase base;
	#ifdef CAPTURE_ARG_PATHS
	int index;
	uint32_t n_ref;
	uint16_t cpu;
	#endif
	long length;
} __attribute__((packed)) EventTruncate;

typedef struct event_ftruncate_t {
	EventBaseFD base_fd_info;
	long length;
} __attribute__((packed)) EventFTruncate;

typedef struct event_base_path_t {
	EventBase base;
	#ifdef CAPTURE_ARG_PATHS
	int index;
	uint32_t n_ref;
	uint16_t cpu;
	#endif
	int flag;
} __attribute__((packed)) EventBasePath;

typedef struct event_xattr_t {
	EventBase base;
	#ifdef CAPTURE_ARG_PATHS
	int index;
	uint32_t n_ref;
	uint16_t cpu;
	#endif
	char name[MAX_ATTR_NAME_SIZE];
	int flag;
} __attribute__((packed)) EventXattr;

typedef struct event_xattr_fd_t {
	EventBaseFD base_fd_info;
	char name[MAX_ATTR_NAME_SIZE];
	int flag;
} __attribute__((packed)) EventXattrFD;

typedef struct event_mknod_t {
	EventBase base;
	#ifdef CAPTURE_ARG_PATHS
	int index;
	uint32_t n_ref;
	uint16_t cpu;
	#endif
	uint16_t mode;
	uint16_t dev;
} __attribute__((packed)) EventMknod;

typedef struct data_info_t {
	uint64_t bytes_request;
	long offset;
	#if COMPUTE_HASH==1
	uint64_t captured_size;
	int index;
	int n_ref;
	#endif
	#if COMPUTE_HASH==2
	uint64_t captured_size;
	uint32_t hash;
	#endif
} __attribute__((packed)) DataInfo;

typedef struct event_storage_data_t {
 	EventBaseFD base_fd_info;
	uint16_t file_type;
	SocketData sock_data;
	DataInfo data;
} __attribute__((packed)) EventStorageData;

typedef struct event_readahead {
	EventBaseFD base_fd_info;
	long offset;
	size_t count;
} __attribute__((packed)) EventReadahead;

typedef struct event_lseek {
	EventBaseFD base_fd_info;
	long offset;
	uint32_t whence;
} __attribute__((packed)) EventLSeek;


typedef struct event_socket_t {
	EventBaseFD base_fd_info;
	uint32_t s_family;
	uint32_t s_type;
	uint32_t s_protocol;
	int32_t second_fd;
} __attribute__((packed)) EventSocket;

typedef struct event_network_t {
	EventBaseFD base_fd_info;
	#ifdef TRACE_SOCKADDR
	unsigned short family;
	u32 addr_len;
	union {
		Unix_Addr un;
		Inet_Addr in;
		Netlink_Addr nl;
	} __attribute__((packed));
	#endif
} __attribute__((packed)) EventNetwork;

typedef struct event_accept_t {
	EventBaseFD base_fd_info;
	SocketData sock_data;
	#ifdef TRACE_SOCKADDR
	unsigned short family;
	u32 addr_len;
	union {
		Unix_Addr un;
		Inet_Addr in;
		Netlink_Addr nl;
	} __attribute__((packed));
	#endif
	int flags;
} __attribute__((packed)) EventAccept;

typedef struct event_network_sockopt_t {
	EventBaseFD base_fd_info;
	int level;
	int optname;
} __attribute__((packed)) EventSockopt;

typedef struct event_network_data_t {
 	EventNetwork event_network;
	uint32_t flags;
	SocketData sock_data;
	DataInfo data;
} __attribute__((packed)) EventNetworkData;

typedef struct event_network_listen_t {
	EventBaseFD base_fd_info;
	int backlog;
} __attribute__((packed)) event_network_listen_t;

// ------

typedef struct event_t {
    union {
        EventStorageData event_storage_data;
        EventNetwork event_network;
		EventAccept event_network_accept;
		EventNetworkData event_network_data;
    };
} event_t;

// ------

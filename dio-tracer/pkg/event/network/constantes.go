package events

// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <linux/netlink.h>
// #include <linux/tcp.h>
// #include <linux/socket.h>
import "C"

const (
	SizeofSockaddrInet4 int32 = C.sizeof_struct_sockaddr_in
	SizeofSockaddrInet6 int32 = C.sizeof_struct_sockaddr_in6
)

var SocketType = map[int]string{
	C.SOCK_STREAM:    "SOCK_STREAM",
	C.SOCK_DGRAM:     "SOCK_DGRAM",
	C.SOCK_RAW:       "SOCK_RAW",
	C.SOCK_RDM:       "SOCK_RDM",
	C.SOCK_SEQPACKET: "SOCK_SEQPACKET",
	C.SOCK_PACKET:    "SOCK_PACKET",
	C.SOCK_DCCP:      "SOCK_DCCP",
}

var SocketAddressFamilies = map[int]string{
	C.AF_UNSPEC:     "AF_UNSPEC",
	C.AF_UNIX:       "AF_UNIX",
	C.AF_INET:       "AF_INET",
	C.AF_AX25:       "AF_AX25",
	C.AF_IPX:        "AF_IPX",
	C.AF_APPLETALK:  "AF_APPLETALK",
	C.AF_NETROM:     "AF_NETROM",
	C.AF_BRIDGE:     "AF_BRIDGE",
	C.AF_ATMPVC:     "AF_ATMPVC",
	C.AF_X25:        "AF_X25",
	C.AF_INET6:      "AF_INET6",
	C.AF_ROSE:       "AF_ROSE",
	C.AF_DECnet:     "AF_DECnet",
	C.AF_NETBEUI:    "AF_NETBEUI",
	C.AF_SECURITY:   "AF_SECURITY",
	C.AF_KEY:        "AF_KEY",
	C.AF_NETLINK:    "AF_NETLINK",
	C.AF_PACKET:     "AF_PACKET",
	C.AF_ASH:        "AF_ASH",
	C.AF_ECONET:     "AF_ECONET",
	C.AF_ATMSVC:     "AF_ATMSVC",
	C.AF_RDS:        "AF_RDS",
	C.AF_SNA:        "AF_SNA",
	C.AF_IRDA:       "AF_IRDA",
	C.AF_PPPOX:      "AF_PPPOX",
	C.AF_WANPIPE:    "AF_WANPIPE",
	C.AF_LLC:        "AF_LLC",
	C.AF_IB:         "AF_IB",
	C.AF_MPLS:       "AF_MPLS",
	C.AF_CAN:        "AF_CAN",
	C.AF_TIPC:       "AF_TIPC",
	C.AF_BLUETOOTH:  "AF_BLUETOOTH",
	C.AF_IUCV:       "AF_IUCV",
	C.AF_RXRPC:      "AF_RXRPC",
	C.AF_ISDN:       "AF_ISDN",
	C.AF_PHONET:     "AF_PHONET",
	C.AF_IEEE802154: "AF_IEEE802154",
	C.AF_CAIF:       "AF_CAIF",
	C.AF_ALG:        "AF_ALG",
	C.AF_NFC:        "AF_NFC",
	C.AF_VSOCK:      "AF_VSOCK",
	C.AF_KCM:        "AF_KCM",
	C.AF_QIPCRTR:    "AF_QIPCRTR",
	C.AF_SMC:        "AF_SMC",
}

var IPProtocols = map[int]string{
	C.IPPROTO_IP:      "IPPROTO_IP",
	C.IPPROTO_ICMP:    "IPPROTO_ICMP",
	C.IPPROTO_IGMP:    "IPPROTO_IGMP",
	C.IPPROTO_IPIP:    "IPPROTO_IPIP",
	C.IPPROTO_TCP:     "IPPROTO_TCP",
	C.IPPROTO_EGP:     "IPPROTO_EGP",
	C.IPPROTO_PUP:     "IPPROTO_PUP",
	C.IPPROTO_UDP:     "IPPROTO_UDP",
	C.IPPROTO_IDP:     "IPPROTO_IDP",
	C.IPPROTO_TP:      "IPPROTO_TP",
	C.IPPROTO_DCCP:    "IPPROTO_DCCP",
	C.IPPROTO_IPV6:    "IPPROTO_IPV6",
	C.IPPROTO_RSVP:    "IPPROTO_RSVP",
	C.IPPROTO_GRE:     "IPPROTO_GRE",
	C.IPPROTO_ESP:     "IPPROTO_ESP",
	C.IPPROTO_AH:      "IPPROTO_AH",
	C.IPPROTO_MTP:     "IPPROTO_MTP",
	C.IPPROTO_BEETPH:  "IPPROTO_BEETPH",
	C.IPPROTO_ENCAP:   "IPPROTO_ENCAP",
	C.IPPROTO_PIM:     "IPPROTO_PIM",
	C.IPPROTO_COMP:    "IPPROTO_COMP",
	C.IPPROTO_SCTP:    "IPPROTO_SCTP",
	C.IPPROTO_UDPLITE: "IPPROTO_UDPLITE",
	C.IPPROTO_MPLS:    "IPPROTO_MPLS",
	C.IPPROTO_RAW:     "IPPROTO_RAW",
	C.IPPROTO_MAX:     "IPPROTO_MAX",
}

var NetlinkProtocols = map[int]string{
	C.NETLINK_ROUTE:          "NETLINK_ROUTE",
	C.NETLINK_UNUSED:         "NETLINK_UNUSED",
	C.NETLINK_USERSOCK:       "NETLINK_USERSOCK",
	C.NETLINK_FIREWALL:       "NETLINK_FIREWALL",
	C.NETLINK_SOCK_DIAG:      "NETLINK_SOCK_DIAG",
	C.NETLINK_NFLOG:          "NETLINK_NFLOG",
	C.NETLINK_XFRM:           "NETLINK_XFRM",
	C.NETLINK_SELINUX:        "NETLINK_SELINUX",
	C.NETLINK_ISCSI:          "NETLINK_ISCSI",
	C.NETLINK_AUDIT:          "NETLINK_AUDIT",
	C.NETLINK_FIB_LOOKUP:     "NETLINK_FIB_LOOKUP",
	C.NETLINK_CONNECTOR:      "NETLINK_CONNECTOR",
	C.NETLINK_NETFILTER:      "NETLINK_NETFILTER",
	C.NETLINK_IP6_FW:         "NETLINK_IP6_FW",
	C.NETLINK_DNRTMSG:        "NETLINK_DNRTMSG",
	C.NETLINK_KOBJECT_UEVENT: "NETLINK_KOBJECT_UEVENT",
	C.NETLINK_GENERIC:        "NETLINK_GENERIC",
	C.NETLINK_SCSITRANSPORT:  "NETLINK_SCSITRANSPORT",
	C.NETLINK_ECRYPTFS:       "NETLINK_ECRYPTFS",
	C.NETLINK_RDMA:           "NETLINK_RDMA",
	C.NETLINK_CRYPTO:         "NETLINK_CRYPTO",
	C.NETLINK_SMC:            "NETLINK_SMC",
}

var IPsocketOptions = map[int]string{
	C.IP_TOS:                    "IP_TOS",
	C.IP_TTL:                    "IP_TTL",
	C.IP_HDRINCL:                "IP_HDRINCL",
	C.IP_OPTIONS:                "IP_OPTIONS",
	C.IP_ROUTER_ALERT:           "IP_ROUTER_ALERT",
	C.IP_RECVOPTS:               "IP_RECVOPTS",
	C.IP_RETOPTS:                "IP_RETOPTS",
	C.IP_PKTINFO:                "IP_PKTINFO",
	C.IP_PKTOPTIONS:             "IP_PKTOPTIONS",
	C.IP_MTU_DISCOVER:           "IP_MTU_DISCOVER",
	C.IP_RECVERR:                "IP_RECVERR",
	C.IP_RECVTTL:                "IP_RECVTTL",
	C.IP_RECVTOS:                "IP_RECVTOS",
	C.IP_MTU:                    "IP_MTU",
	C.IP_FREEBIND:               "IP_FREEBIND",
	C.IP_IPSEC_POLICY:           "IP_IPSEC_POLICY",
	C.IP_XFRM_POLICY:            "IP_XFRM_POLICY",
	C.IP_PASSSEC:                "IP_PASSSEC",
	C.IP_TRANSPARENT:            "IP_TRANSPARENT",
	C.IP_ORIGDSTADDR:            "IP_ORIGDSTADDR",
	C.IP_MINTTL:                 "IP_MINTTL",
	C.IP_NODEFRAG:               "IP_NODEFRAG",
	C.IP_CHECKSUM:               "IP_CHECKSUM",
	C.IP_BIND_ADDRESS_NO_PORT:   "IP_BIND_ADDRESS_NO_PORT",
	C.IP_RECVFRAGSIZE:           "IP_RECVFRAGSIZE",
	C.IP_PMTUDISC_DONT:          "IP_PMTUDISC_DONT",
	C.IP_MULTICAST_IF:           "IP_MULTICAST_IF",
	C.IP_MULTICAST_TTL:          "IP_MULTICAST_TTL",
	C.IP_MULTICAST_LOOP:         "IP_MULTICAST_LOOP",
	C.IP_ADD_MEMBERSHIP:         "IP_ADD_MEMBERSHIP",
	C.IP_DROP_MEMBERSHIP:        "IP_DROP_MEMBERSHIP",
	C.IP_UNBLOCK_SOURCE:         "IP_UNBLOCK_SOURCE",
	C.IP_BLOCK_SOURCE:           "IP_BLOCK_SOURCE",
	C.IP_ADD_SOURCE_MEMBERSHIP:  "IP_ADD_SOURCE_MEMBERSHIP",
	C.IP_DROP_SOURCE_MEMBERSHIP: "IP_DROP_SOURCE_MEMBERSHIP",
	C.IP_MSFILTER:               "IP_MSFILTER",
	C.MCAST_JOIN_GROUP:          "MCAST_JOIN_GROUP",
	C.MCAST_BLOCK_SOURCE:        "MCAST_BLOCK_SOURCE",
	C.MCAST_UNBLOCK_SOURCE:      "MCAST_UNBLOCK_SOURCE",
	C.MCAST_LEAVE_GROUP:         "MCAST_LEAVE_GROUP",
	C.MCAST_JOIN_SOURCE_GROUP:   "MCAST_JOIN_SOURCE_GROUP",
	C.MCAST_LEAVE_SOURCE_GROUP:  "MCAST_LEAVE_SOURCE_GROUP",
	C.MCAST_MSFILTER:            "MCAST_MSFILTER",
	C.IP_MULTICAST_ALL:          "IP_MULTICAST_ALL",
	C.IP_UNICAST_IF:             "IP_UNICAST_IF",
}

var TCPsocketOptions = map[int]string{
	C.TCP_NODELAY:              "TCP_NODELAY",              /* Turn off Nagle's algorithm. */
	C.TCP_MAXSEG:               "TCP_MAXSEG",               /* Limit MSS */
	C.TCP_CORK:                 "TCP_CORK",                 /* Never send partially complete segments */
	C.TCP_KEEPIDLE:             "TCP_KEEPIDLE",             /* Start keeplives after this period */
	C.TCP_KEEPINTVL:            "TCP_KEEPINTVL",            /* Interval between keepalives */
	C.TCP_KEEPCNT:              "TCP_KEEPCNT",              /* Number of keepalives before death */
	C.TCP_SYNCNT:               "TCP_SYNCNT",               /* Number of SYN retransmits */
	C.TCP_LINGER2:              "TCP_LINGER2",              /* Life time of orphaned FIN-WAIT-2 state */
	C.TCP_DEFER_ACCEPT:         "TCP_DEFER_ACCEPT",         /* Wake up listener only when data arrive */
	C.TCP_WINDOW_CLAMP:         "TCP_WINDOW_CLAMP",         /* Bound advertised window */
	C.TCP_INFO:                 "TCP_INFO",                 /* Information about this connection. */
	C.TCP_QUICKACK:             "TCP_QUICKACK",             /* Block/reenable quick acks */
	C.TCP_CONGESTION:           "TCP_CONGESTION",           /* Congestion control algorithm */
	C.TCP_MD5SIG:               "TCP_MD5SIG",               /* TCP MD5 Signature (RFC2385) */
	C.TCP_THIN_LINEAR_TIMEOUTS: "TCP_THIN_LINEAR_TIMEOUTS", /* Use linear timeouts for thin streams*/
	C.TCP_THIN_DUPACK:          "TCP_THIN_DUPACK",          /* Fast retrans. after 1 dupack */
	C.TCP_USER_TIMEOUT:         "TCP_USER_TIMEOUT",         /* How long for loss retry before timeout */
	C.TCP_REPAIR:               "TCP_REPAIR",               /* TCP sock is under repair right now */
	C.TCP_REPAIR_QUEUE:         "TCP_REPAIR_QUEUE",
	C.TCP_QUEUE_SEQ:            "TCP_QUEUE_SEQ",
	C.TCP_REPAIR_OPTIONS:       "TCP_REPAIR_OPTIONS",
	C.TCP_FASTOPEN:             "TCP_FASTOPEN", /* Enable FastOpen on listeners */
	C.TCP_TIMESTAMP:            "TCP_TIMESTAMP",
	C.TCP_NOTSENT_LOWAT:        "TCP_NOTSENT_LOWAT",      /* limit number of unsent bytes in write queue */
	C.TCP_CC_INFO:              "TCP_CC_INFO",            /* Get Congestion Control (optional) info */
	C.TCP_SAVE_SYN:             "TCP_SAVE_SYN",           /* Record SYN headers for new connections */
	C.TCP_SAVED_SYN:            "TCP_SAVED_SYN",          /* Get SYN headers recorded for connection */
	C.TCP_REPAIR_WINDOW:        "TCP_REPAIR_WINDOW",      /* Get/set window parameters */
	C.TCP_FASTOPEN_CONNECT:     "TCP_FASTOPEN_CONNECT",   /* Attempt FastOpen with connect */
	C.TCP_ULP:                  "TCP_ULP",                /* Attach a ULP to a TCP connection */
	C.TCP_MD5SIG_EXT:           "TCP_MD5SIG_EXT",         /* TCP MD5 Signature with extensions */
	C.TCP_FASTOPEN_KEY:         "TCP_FASTOPEN_KEY",       /* Set the key for Fast Open (cookie) */
	C.TCP_FASTOPEN_NO_COOKIE:   "TCP_FASTOPEN_NO_COOKIE", /* Enable TFO without a TFO cookie */
	C.TCP_ZEROCOPY_RECEIVE:     "TCP_ZEROCOPY_RECEIVE",
	C.TCP_INQ:                  "TCP_INQ",      /* Notify bytes available to read as a cmsg on read */
	C.TCP_TX_DELAY:             "TCP_TX_DELAY", /* delay outgoing packets by XX usec */
}

var SocketOptions = map[int]string{
	C.SO_DEBUG:                         "SO_DEBUG",
	C.SO_REUSEADDR:                     "SO_REUSEADDR",
	C.SO_KEEPALIVE:                     "SO_KEEPALIVE",
	C.SO_DONTROUTE:                     "SO_DONTROUTE",
	C.SO_BROADCAST:                     "SO_BROADCAST",
	C.SO_LINGER:                        "SO_LINGER",
	C.SO_OOBINLINE:                     "SO_OOBINLINE",
	C.SO_REUSEPORT:                     "SO_REUSEPORT",
	C.SO_TYPE:                          "SO_TYPE",
	C.SO_ERROR:                         "SO_ERROR",
	C.SO_SNDBUF:                        "SO_SNDBUF",
	C.SO_RCVBUF:                        "SO_RCVBUF",
	C.SO_SNDBUFFORCE:                   "SO_SNDBUFFORCE",
	C.SO_RCVBUFFORCE:                   "SO_RCVBUFFORCE",
	C.SO_RCVLOWAT:                      "SO_RCVLOWAT",
	C.SO_SNDLOWAT:                      "SO_SNDLOWAT",
	C.SO_RCVTIMEO_OLD:                  "SO_RCVTIMEO_OLD",
	C.SO_SNDTIMEO_OLD:                  "SO_SNDTIMEO_OLD",
	C.SO_ACCEPTCONN:                    "SO_ACCEPTCONN",
	C.SO_PROTOCOL:                      "SO_PROTOCOL",
	C.SO_DOMAIN:                        "SO_DOMAIN",
	C.SO_NO_CHECK:                      "SO_NO_CHECK",
	C.SO_PRIORITY:                      "SO_PRIORITY",
	C.SO_BSDCOMPAT:                     "SO_BSDCOMPAT",
	C.SO_PASSCRED:                      "SO_PASSCRED",
	C.SO_PEERCRED:                      "SO_PEERCRED",
	C.SO_BINDTODEVICE:                  "SO_BINDTODEVICE",
	C.SO_ATTACH_FILTER:                 "SO_ATTACH_FILTER",
	C.SO_DETACH_FILTER:                 "SO_DETACH_FILTER",
	C.SO_PEERNAME:                      "SO_PEERNAME",
	C.SO_PEERSEC:                       "SO_PEERSEC",
	C.SO_PASSSEC:                       "SO_PASSSEC",
	C.SO_SECURITY_AUTHENTICATION:       "SO_SECURITY_AUTHENTICATION",
	C.SO_SECURITY_ENCRYPTION_TRANSPORT: "SO_SECURITY_ENCRYPTION_TRANSPORT",
	C.SO_SECURITY_ENCRYPTION_NETWORK:   "SO_SECURITY_ENCRYPTION_NETWORK",
	C.SO_MARK:                          "SO_MARK",
	C.SO_RXQ_OVFL:                      "SO_RXQ_OVFL",
	C.SO_WIFI_STATUS:                   "SO_WIFI_STATUS",
	C.SO_PEEK_OFF:                      "SO_PEEK_OFF",
	C.SO_NOFCS:                         "SO_NOFCS",
	C.SO_LOCK_FILTER:                   "SO_LOCK_FILTER",
	C.SO_SELECT_ERR_QUEUE:              "SO_SELECT_ERR_QUEUE",
	C.SO_BUSY_POLL:                     "SO_BUSY_POLL",
	C.SO_MAX_PACING_RATE:               "SO_MAX_PACING_RATE",
	C.SO_BPF_EXTENSIONS:                "SO_BPF_EXTENSIONS",
	C.SO_INCOMING_CPU:                  "SO_INCOMING_CPU",
	C.SO_ATTACH_BPF:                    "SO_ATTACH_BPF",
	C.SO_ATTACH_REUSEPORT_CBPF:         "SO_ATTACH_REUSEPORT_CBPF",
	C.SO_ATTACH_REUSEPORT_EBPF:         "SO_ATTACH_REUSEPORT_EBPF",
	C.SO_CNX_ADVICE:                    "SO_CNX_ADVICE",
	C.SCM_TIMESTAMPING_OPT_STATS:       "SCM_TIMESTAMPING_OPT_STATS",
	C.SO_MEMINFO:                       "SO_MEMINFO",
	C.SO_INCOMING_NAPI_ID:              "SO_INCOMING_NAPI_ID",
	C.SO_COOKIE:                        "SO_COOKIE",
	C.SCM_TIMESTAMPING_PKTINFO:         "SCM_TIMESTAMPING_PKTINFO",
	C.SO_PEERGROUPS:                    "SO_PEERGROUPS",
	C.SO_ZEROCOPY:                      "SO_ZEROCOPY",
	C.SO_TXTIME:                        "SO_TXTIME",
	C.SO_BINDTOIFINDEX:                 "SO_BINDTOIFINDEX",
	C.SO_TIMESTAMP_OLD:                 "SO_TIMESTAMP_OLD",
	C.SO_TIMESTAMPNS_OLD:               "SO_TIMESTAMPNS_OLD",
	C.SO_TIMESTAMPING_OLD:              "SO_TIMESTAMPING_OLD",
	C.SO_TIMESTAMP_NEW:                 "SO_TIMESTAMP_NEW",
	C.SO_TIMESTAMPNS_NEW:               "SO_TIMESTAMPNS_NEW",
	C.SO_TIMESTAMPING_NEW:              "SO_TIMESTAMPING_NEW",
	C.SO_RCVTIMEO_NEW:                  "SO_RCVTIMEO_NEW",
	C.SO_SNDTIMEO_NEW:                  "SO_SNDTIMEO_NEW",
	C.SO_DETACH_REUSEPORT_BPF:          "SO_DETACH_REUSEPORT_BPF",
}

var accept4Flags = map[int]string{
	C.SOCK_NONBLOCK: "SOCK_NONBLOCK",
	C.SOCK_CLOEXEC:  "SOCK_CLOEXEC",
}

var recvFlags = map[int]string{
	C.MSG_CMSG_CLOEXEC: "MSG_CMSG_CLOEXEC",
	C.MSG_DONTWAIT:     "MSG_DONTWAIT",
	C.MSG_ERRQUEUE:     "MSG_ERRQUEUE",
	C.MSG_OOB:          "MSG_OOB",
	C.MSG_PEEK:         "MSG_PEEK",
	C.MSG_TRUNC:        "MSG_TRUNC",
	C.MSG_WAITALL:      "MSG_WAITALL",
}

var sendFlags = map[int]string{
	C.MSG_CONFIRM:   "MSG_CONFIRM",
	C.MSG_DONTROUTE: "MSG_DONTROUTE",
	C.MSG_DONTWAIT:  "MSG_DONTWAIT",
	C.MSG_EOR:       "MSG_EOR",
	C.MSG_MORE:      "MSG_MORE",
	C.MSG_NOSIGNAL:  "MSG_NOSIGNAL",
	C.MSG_OOB:       "MSG_OOB",
}

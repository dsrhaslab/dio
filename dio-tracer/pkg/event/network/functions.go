package events

// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <linux/netlink.h>
// #include <linux/tcp.h>
// #include <linux/socket.h>
import "C"
import (
	"fmt"
	"net"
	"unsafe"
)

func GetAddressFamilyName(s_family int) string {
	af_name, ok := SocketAddressFamilies[s_family]
	if !ok {
		return fmt.Sprintf("Unknown: %d", s_family)
	}
	return af_name
}

func GetProtocolName(s_family int, s_protocol int) string {

	var proto_name string
	switch s_family {
	case C.AF_INET, C.AF_INET6:
		proto_name = IPProtocols[s_protocol]
	case C.AF_NETLINK:
		proto_name = NetlinkProtocols[s_protocol]
	default:
		proto_name = fmt.Sprintf("%v", s_protocol)
	}
	return proto_name
}

func GetSockLevelAndOptName(level int, opt int) (string, string) {
	var levelname, optname string

	switch level {
	case C.SOL_SOCKET:
		levelname = "SOL_SOCKET"
		optname = SocketOptions[opt]
	case C.IPPROTO_IP:
		levelname = "IPPROTO_IP"
		optname = IPsocketOptions[opt]
	case C.IPPROTO_TCP:
		levelname = "IPPROTO_TCP"
		optname = TCPsocketOptions[opt]
	default:
		levelname = fmt.Sprintf("%v", level)
		optname = fmt.Sprintf("%v", opt)
	}
	return levelname, optname
}

func GetSocketTypeName(s_socketType uint32) string {
	var socket_type_str string
	socket_type_num := s_socketType & 0xf
	socket_type_str, ok := SocketType[int(socket_type_num)]
	if !ok {
		return "Unknown"
	}
	flags := s_socketType &^ 0xf
	if flags&C.SOCK_CLOEXEC == C.SOCK_CLOEXEC {
		socket_type_str = fmt.Sprintf("%s|SOCK_CLOEXEC", socket_type_str)
	}

	if flags&C.SOCK_NONBLOCK == C.SOCK_NONBLOCK {
		socket_type_str = fmt.Sprintf("%s|SOCK_NONBLOCK", socket_type_str)
	}
	return socket_type_str
}

func ipv4ToString(address *uint64) string {
	var src = make([]byte, C.INET_ADDRSTRLEN)
	C.inet_ntop(C.AF_INET, unsafe.Pointer(address), (*C.char)(unsafe.Pointer(&src)), C.INET_ADDRSTRLEN)
	return net.ParseIP(C.GoString((*C.char)(unsafe.Pointer(&src)))).String()
}

func ipv6ToString(address *[2]uint64) string {
	var src = make([]byte, C.INET6_ADDRSTRLEN)
	C.inet_ntop(C.AF_INET6, unsafe.Pointer(address), (*C.char)(unsafe.Pointer(&src)), C.INET6_ADDRSTRLEN)
	return net.ParseIP(C.GoString((*C.char)(unsafe.Pointer(&src)))).String()
}

func ParseSocketAddress(family int, addr *[2]uint64) string {
	var addr_str string
	if family == C.AF_INET {
		addr_str = ipv4ToString(&addr[1])
	} else if family == C.AF_INET6 {
		addr_str = fmt.Sprintf("[%s]", ipv6ToString(addr))
	}
	return addr_str
}

func ParseSunPath(pathBytes []byte) string {
	len := -1
	for i, b := range pathBytes {
		if b == 0 {
			break
		}
		len = i
	}
	len += 1
	pathCstr := (*C.char)(unsafe.Pointer(&pathBytes[0]))
	return fmt.Sprintf("%s", C.GoStringN(pathCstr, C.int(len)))
}

func getNetworkFlagsStr(flags_string string, flags int, map_flags map[int]string) string {
	for key, val := range map_flags {
		if flags&key == key {
			flags = flags &^ key
			if flags_string == "" {
				flags_string = fmt.Sprintf("%v", val)
			} else {
				flags_string = fmt.Sprintf("%s|%v", flags_string, val)
			}
		}
	}
	return flags_string
}

func GetAccept4Flags(flags int) string {
	return getNetworkFlagsStr("", flags, accept4Flags)
}

func GetRecvFlags(flags int) string {
	return getNetworkFlagsStr("", flags, recvFlags)
}

func GetSendFlags(flags int) string {
	return getNetworkFlagsStr("", flags, sendFlags)
}

func GenerateSockAddress(s_family uint16, saddr [2]uint64, sport uint16, daddr [2]uint64, dport uint16) string {
	var sockaddr string
	saddrstr := ParseSocketAddress(int(s_family), &saddr)
	daddrstr := ParseSocketAddress(int(s_family), &daddr)
	if saddr[0] < daddr[0] {
		sockaddr = fmt.Sprintf("%s:%d-%s:%d", saddrstr, sport, daddrstr, dport)
	} else if daddr[0] < saddr[0] {
		sockaddr = fmt.Sprintf("%s:%d-%s:%d", daddrstr, dport, saddrstr, sport)
	} else if saddr[1] < daddr[1] {
		sockaddr = fmt.Sprintf("%s:%d-%s:%d", saddrstr, sport, daddrstr, dport)
	} else if daddr[1] < saddr[1] {
		sockaddr = fmt.Sprintf("%s:%d-%s:%d", daddrstr, dport, saddrstr, sport)
	} else if sport < dport {
		sockaddr = fmt.Sprintf("%s:%d-%s:%d", saddrstr, sport, daddrstr, dport)
	} else {
		sockaddr = fmt.Sprintf("%s:%d-%s:%d", daddrstr, dport, saddrstr, sport)
	}
	return sockaddr
}

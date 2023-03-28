package tracer

import (
	"bytes"
	"fmt"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/ebpfs"
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
	ebase "github.com/dsrhaslab/dio/dio-tracer/pkg/event/base"
	enetwork "github.com/dsrhaslab/dio/dio-tracer/pkg/event/network"
	epath "github.com/dsrhaslab/dio/dio-tracer/pkg/event/path"
	eprocess "github.com/dsrhaslab/dio/dio-tracer/pkg/event/process"
	eraw "github.com/dsrhaslab/dio/dio-tracer/pkg/event/raw"
	estorage "github.com/dsrhaslab/dio/dio-tracer/pkg/event/storage"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"
	"github.com/iovisor/gobpf/bcc"
)

func (tracer *Tracer) ParseEventPath(dataBuff *bytes.Buffer) (*epath.EventPath, error) {
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))

	index := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	ref := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	cpu := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))

	file, err := tracer.bpfConf.GetFileInfo(index, cpu, ref, ino)
	if err != nil {
		if utils.LoggerConf.DebugMode {
			utils.DebugLogger.Println(err)
		}
		tracer.stats.UpdateIncompleteEvents(event.DIO_PATH_EVENT)
		return nil, err
	}
	file.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	file.SessionID = tracer.conf.SessionName
	return file, nil
}

func (tracer *Tracer) ParseEventRaw(dataBuff *bytes.Buffer, context *event.EventContext) (*eraw.EventRaw, error) {
	new_event := &eraw.EventRaw{}

	new_event.Args = make(map[string]interface{})
	for _, arg := range event.GetEventArgs(context.Etype) {
		data := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		if arg.Type == "int16" {
			val := int16(data)
			new_event.Args[arg.Name] = val
		}
		if arg.Type == "uint16" {
			val := uint16(data)
			new_event.Args[arg.Name] = val
		}
		if arg.Type == "int32" {
			val := int32(data)
			new_event.Args[arg.Name] = val
		}
		if arg.Type == "uint32" {
			val := uint32(data)
			new_event.Args[arg.Name] = val
		}
		if arg.Type == "int64" {
			val := int64(data)
			new_event.Args[arg.Name] = val
		}
		if arg.Type == "uint64" {
			val := data
			new_event.Args[arg.Name] = val
		}
		if arg.Type == "pointer" {
			val := uint64(data)
			str_val := fmt.Sprintf("0x%08x", val)
			new_event.Args[arg.Name+"_ptr"] = str_val
		}
	}
	return new_event, nil
}

func (tracer *Tracer) ParseStorageOpenEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}

	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	new_event.StorageArgs = &estorage.StorageArgs{}

	if *context.ReturnValue >= 0 {
		new_event.StorageArgs.FileDescriptor = &file_descriptor
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}

	flags := int(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	if context.Etype == event.DIO_CREAT {
		new_event.StorageArgs.Flags = ""
	} else {
		new_event.StorageArgs.Flags = estorage.GetFlags(flags)
	}
	mode := int(bcc.GetHostByteOrder().Uint16(dataBuff.Next(2)))
	mode_str := estorage.GetCreationMode(flags, mode)
	if mode_str != "" {
		new_event.StorageArgs.Mode = mode_str
	}

	return new_event, nil
}

func (tracer *Tracer) ParseStorageDataEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	var new_event = &estorage.StorageEvent{}
	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.StorageArgs = &estorage.StorageArgs{FileDescriptor: &file_descriptor}
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}
	file_type := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))

	if tracer.conf.DetailedWithSockData {
		if estorage.GetFileType(file_type) == "Socket" {
			var saddr, daddr [2]uint64
			sfamily := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
			saddr[0] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			saddr[1] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			sport := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
			daddr[0] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			daddr[1] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			dport := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
			new_event.SocketData = &event.SocketData{}
			new_event.SocketData.SocketFamily = enetwork.GetAddressFamilyName(int(sfamily))
			if new_event.SocketData.SocketFamily == "AF_INET" || new_event.SocketData.SocketFamily == "AF_INET6" {
				new_event.SocketData.SocketAddress = enetwork.GenerateSockAddress(sfamily, saddr, sport, daddr, dport)
			}
		} else {
			dataBuff.Next(38)
		}
	}

	content, offset, err := tracer.ParseDataEvent(dataBuff, context.Etype, *context.ReturnValue, context.CPU)
	if err == nil {
		new_event.StorageArgs.EventContent = content
		if offset != -1 {
			new_event.StorageArgs.Offset = &offset
		}
	}
	return new_event, err
}

func (tracer *Tracer) ParseFDEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}
	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.StorageArgs = &estorage.StorageArgs{FileDescriptor: &file_descriptor}
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}
	return new_event, nil
}

func (tracer *Tracer) ParseNetworkSocketEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*enetwork.NetworkEvent, error) {
	new_event := &enetwork.NetworkEvent{}

	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	new_event.NetworkArgs = &enetwork.NetworkArgs{FileDescriptor: &file_descriptor}

	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}

	s_family := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	new_event.NetworkArgs.Family = enetwork.GetAddressFamilyName(int(s_family))

	s_socketType := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	new_event.NetworkArgs.SocketType = enetwork.GetSocketTypeName(s_socketType)

	s_protocol := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	new_event.NetworkArgs.Protocol = enetwork.GetProtocolName(int(s_family), int(s_protocol))

	if context.Etype == event.DIO_SOCKETPAIR {
		new_event.NetworkArgs.FileDescriptor = &file_descriptor
		second_file_descriptor := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		new_event.NetworkArgs.SecondFileDescriptor = &second_file_descriptor
	}

	return new_event, nil
}

func (tracer *Tracer) ParseNetworkBindEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*enetwork.NetworkEvent, error) {
	new_event := &enetwork.NetworkEvent{}
	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.NetworkArgs = &enetwork.NetworkArgs{FileDescriptor: &file_descriptor}
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}

	if tracer.conf.DetailedWithSockAddr {
		s_family := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		new_event.NetworkArgs.Family = enetwork.GetAddressFamilyName(int(s_family))
		addr_len := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		new_event.NetworkArgs.AddrLen = &addr_len

		switch new_event.Family {
		case "AF_UNIX":
			pathBytes := dataBuff.Next(ebpfs.MY_UNIX_PATH_MAX)
			new_event.NetworkArgs.UnPath = enetwork.ParseSunPath(pathBytes)
		case "AF_INET", "AF_INET6":
			if (addr_len < enetwork.SizeofSockaddrInet4) || (addr_len < enetwork.SizeofSockaddrInet6) {
				break
			}
			var addr [2]uint64
			addr[0] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			addr[1] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			dport := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
			new_event.NetworkArgs.InPort = &dport
			new_event.NetworkArgs.InAddr = enetwork.ParseSocketAddress(int(s_family), &addr)
		case "AF_NETLINK":
			portId := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
			new_event.NetworkArgs.NlPid = &portId
			groups := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
			new_event.NetworkArgs.NlGroups = &groups
		}
	}

	return new_event, nil
}

func (tracer *Tracer) ParseNetworkListenEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*enetwork.NetworkEvent, error) {
	new_event := &enetwork.NetworkEvent{}
	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.NetworkArgs = &enetwork.NetworkArgs{FileDescriptor: &file_descriptor}
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}
	backlog := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.NetworkArgs.Backlog = &backlog
	return new_event, nil
}

func (tracer *Tracer) ParseNetworkConnectAcceptEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*enetwork.NetworkEvent, error) {
	new_event := &enetwork.NetworkEvent{}
	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.NetworkArgs = &enetwork.NetworkArgs{FileDescriptor: &file_descriptor}
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}

	if tracer.conf.DetailedWithSockData {
		var saddr, daddr [2]uint64
		sfamily := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		saddr[0] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		saddr[1] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		sport := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		daddr[0] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		daddr[1] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		dport := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		new_event.SocketData = &event.SocketData{}
		new_event.SocketData.SocketFamily = enetwork.GetAddressFamilyName(int(sfamily))
		if new_event.SocketData.SocketFamily == "AF_INET" || new_event.SocketData.SocketFamily == "AF_INET6" {
			new_event.SocketData.SocketAddress = enetwork.GenerateSockAddress(sfamily, saddr, sport, daddr, dport)
		}
	}

	if tracer.conf.DetailedWithSockAddr {
		family := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		new_event.NetworkArgs.Family = enetwork.GetAddressFamilyName(int(family))
		addr_len := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		new_event.NetworkArgs.AddrLen = &addr_len

		switch new_event.NetworkArgs.Family {
		case "AF_UNIX":
			pathBytes := dataBuff.Next(ebpfs.MY_UNIX_PATH_MAX)
			new_event.NetworkArgs.UnPath = enetwork.ParseSunPath(pathBytes)
		case "AF_INET", "AF_INET6":
			if (addr_len < enetwork.SizeofSockaddrInet4) || (addr_len < enetwork.SizeofSockaddrInet6) {
				break
			}
			var addr [2]uint64
			addr[0] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			addr[1] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			dport := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
			new_event.NetworkArgs.InPort = &dport
			new_event.NetworkArgs.InAddr = enetwork.ParseSocketAddress(int(family), &addr)
		case "AF_NETLINK":
			portId := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
			new_event.NetworkArgs.NlPid = &portId
			groups := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
			new_event.NetworkArgs.NlGroups = &groups
		}
	}

	if context.Etype == event.DIO_ACCEPT4 {
		flags := int(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		flags_str := enetwork.GetAccept4Flags(flags)
		new_event.NetworkArgs.Flags = flags_str
	}

	return new_event, nil
}

func (tracer *Tracer) ParseNetworkDataEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*enetwork.NetworkEvent, error) {
	new_event := &enetwork.NetworkEvent{}
	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.NetworkArgs = &enetwork.NetworkArgs{FileDescriptor: &file_descriptor}
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}

	if tracer.conf.DetailedWithSockAddr {
		s_family := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		new_event.Family = enetwork.GetAddressFamilyName(int(s_family))
		addrlen := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		new_event.AddrLen = &addrlen

		switch new_event.Family {
		case "AF_UNIX":
			pathBytes := dataBuff.Next(ebpfs.MY_UNIX_PATH_MAX)
			new_event.UnPath = enetwork.ParseSunPath(pathBytes)
		case "AF_INET", "AF_INET6":
			if (addrlen < enetwork.SizeofSockaddrInet4) || (addrlen < enetwork.SizeofSockaddrInet6) {
				dataBuff.Next(ebpfs.MY_UNIX_PATH_MAX)
				break
			}
			var addr [2]uint64
			addr[0] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			addr[1] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
			port := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
			new_event.InPort = &port
			new_event.InAddr = enetwork.ParseSocketAddress(int(s_family), &addr)
			dataBuff.Next(ebpfs.MY_UNIX_PATH_MAX - 8 - 8 - 2)
		case "AF_NETLINK":
			portId := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
			new_event.NlPid = &portId
			groups := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
			new_event.NlGroups = &groups
			dataBuff.Next(ebpfs.MY_UNIX_PATH_MAX - 4 - 4)
		default:
			dataBuff.Next(ebpfs.MY_UNIX_PATH_MAX)
		}
	}

	flags := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	if context.Etype == event.DIO_RECVFROM || context.Etype == event.DIO_RECVMSG {
		new_event.NetworkArgs.Flags = enetwork.GetRecvFlags(int(flags))
	} else if context.Etype == event.DIO_SENDTO || context.Etype == event.DIO_SENDMSG {
		new_event.NetworkArgs.Flags = enetwork.GetSendFlags(int(flags))
	}

	if tracer.conf.DetailedWithSockData {
		var saddr, daddr [2]uint64
		sfamily := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		saddr[0] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		saddr[1] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		sport := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		daddr[0] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		daddr[1] = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		dport := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		new_event.SocketData = &event.SocketData{}
		new_event.SocketData.SocketFamily = enetwork.GetAddressFamilyName(int(sfamily))
		if new_event.SocketData.SocketFamily == "AF_INET" || new_event.SocketData.SocketFamily == "AF_INET6" {
			new_event.SocketData.SocketAddress = enetwork.GenerateSockAddress(sfamily, saddr, sport, daddr, dport)
		}
	}

	content, _, err := tracer.ParseDataEvent(dataBuff, context.Etype, *context.ReturnValue, context.CPU)
	if err != nil {
		return nil, err
	}
	new_event.EventContent = content
	new_event.FileDescriptor = &file_descriptor
	return new_event, err
}

func (tracer *Tracer) ParseNetworkSockOptEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*enetwork.NetworkEvent, error) {
	new_event := &enetwork.NetworkEvent{}
	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.NetworkArgs = &enetwork.NetworkArgs{FileDescriptor: &file_descriptor}
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}

	level := int(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	optname := int(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.NetworkArgs.Level, new_event.NetworkArgs.OptName = enetwork.GetSockLevelAndOptName(level, optname)

	return new_event, nil
}

func (tracer *Tracer) ParseRenameEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}

	if tracer.conf.DetailedWithArgPaths {
		index := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		ref := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		cpu := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		oldname_len := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		newname_len := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))

		oldname, newname, err := tracer.bpfConf.GetDoublePathArgs(index, cpu, ref, oldname_len, newname_len)
		if err != nil {
			if utils.LoggerConf.DebugMode {
				utils.DebugLogger.Println(err)
			}
			tracer.stats.UpdateIncompleteEvents(context.Etype)
			// return nil, err
		}
		if oldname != "" {
			new_event.PathArg = &estorage.PathArg{Filename: oldname}
		}
		if newname != "" {
			new_event.StorageArgs = &estorage.StorageArgs{NewName: newname}
		}
	}

	flags := int(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	if context.Etype == event.DIO_RENAMEAT2 {
		if new_event.StorageArgs == nil {
			new_event.StorageArgs = &estorage.StorageArgs{}
		}
		new_event.StorageArgs.Flags = estorage.GetRenameFlags(flags)
	}

	return new_event, nil
}

func (tracer *Tracer) ParseReadlinkEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}

	if tracer.conf.DetailedWithArgPaths {
		index := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		ref := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		cpu := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
		path_len := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		buf_len := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))

		path, buf, err := tracer.bpfConf.GetDoublePathArgs(index, cpu, ref, path_len, buf_len)
		if err != nil {
			if utils.LoggerConf.DebugMode {
				utils.DebugLogger.Println(err)
			}
			tracer.stats.UpdateIncompleteEvents(context.Etype)
			// return nil, err
		}
		if path != "" {
			new_event.PathArg = &estorage.PathArg{Filename: path}
		}
		if buf != "" {
			new_event.StorageArgs = &estorage.StorageArgs{Buf: buf}
		}
	}

	return new_event, nil
}

func (tracer *Tracer) ParseTruncateEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}
	hasFd := estorage.HasFileDescriptor(context.Etype)
	if hasFd {
		file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		if new_event.StorageArgs == nil {
			new_event.StorageArgs = &estorage.StorageArgs{}
		}
		new_event.StorageArgs.FileDescriptor = &file_descriptor
		dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		if *context.ReturnValue >= 0 {
			new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
		}
	}

	if !hasFd && tracer.conf.DetailedWithArgPaths {
		index := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		ref := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		cpu := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))

		path, err := tracer.bpfConf.GetPathArgs(index, cpu, ref)
		if err != nil {
			if utils.LoggerConf.DebugMode {
				utils.DebugLogger.Println(err)
			}
			tracer.stats.UpdateIncompleteEvents(context.Etype)
			// return nil, err
		}
		if path != "" {
			new_event.PathArg = &estorage.PathArg{Filename: path}
		}
	}

	length := (bcc.GetHostByteOrder().Uint64(dataBuff.Next(8)))
	if new_event.StorageArgs == nil {
		new_event.StorageArgs = &estorage.StorageArgs{}
	}
	new_event.StorageArgs.TruncLength = &length

	return new_event, nil
}

func (tracer *Tracer) ParseBasePathEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}

	if tracer.conf.DetailedWithArgPaths {
		index := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		ref := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		cpu := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))

		path, err := tracer.bpfConf.GetPathArgs(index, cpu, ref)
		if err != nil {
			if utils.LoggerConf.DebugMode {
				utils.DebugLogger.Println(err)
			}
			tracer.stats.UpdateIncompleteEvents(context.Etype)
			// return nil, err
		}
		if path != "" {
			new_event.PathArg = &estorage.PathArg{Filename: path}
		}
	}

	if context.Etype == event.DIO_UNLINKAT {
		flags := int(int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))))
		flag_str := estorage.GetUnlinkAtFlags(flags)
		if flag_str != "" {
			new_event.StorageArgs = &estorage.StorageArgs{Flags: flag_str}
		}
	} else if context.Etype == event.DIO_FSTATAT {
		flags := int(int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))))
		flag_str := estorage.GetFStatAtFlags(flags)
		if flag_str != "" {
			new_event.StorageArgs = &estorage.StorageArgs{Flags: flag_str}
		}
	}

	return new_event, nil
}

func (tracer *Tracer) ParseMknodEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}

	if tracer.conf.DetailedWithArgPaths {
		index := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		ref := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		cpu := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))

		path, err := tracer.bpfConf.GetPathArgs(index, cpu, ref)
		if err != nil {
			if utils.LoggerConf.DebugMode {
				utils.DebugLogger.Println(err)
			}
			tracer.stats.UpdateIncompleteEvents(context.Etype)
			// return nil, err
		}
		if path != "" {
			new_event.PathArg = &estorage.PathArg{Filename: path}
		}
	}

	if new_event.StorageArgs == nil {
		new_event.StorageArgs = &estorage.StorageArgs{}
	}
	mode := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
	dev := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))
	new_event.StorageArgs.Mode = estorage.GetMknodFType(mode)
	new_event.StorageArgs.Dev = &dev

	return new_event, nil
}

func (tracer *Tracer) ParseXAttrEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}
	hasFd := estorage.HasFileDescriptor(context.Etype)
	if hasFd {
		file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		if new_event.StorageArgs == nil {
			new_event.StorageArgs = &estorage.StorageArgs{}
		}
		new_event.StorageArgs.FileDescriptor = &file_descriptor
		dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		if *context.ReturnValue >= 0 {
			new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
		}
	}

	if !hasFd && tracer.conf.DetailedWithArgPaths {
		index := (bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
		ref := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		cpu := bcc.GetHostByteOrder().Uint16(dataBuff.Next(2))

		path, err := tracer.bpfConf.GetPathArgs(index, cpu, ref)
		if err != nil {
			if utils.LoggerConf.DebugMode {
				utils.DebugLogger.Println(err)
			}
			tracer.stats.UpdateIncompleteEvents(context.Etype)
			// return nil, err
		}
		if path != "" {
			new_event.PathArg = &estorage.PathArg{Filename: path}
		}
	}

	name := tracer.bpfConf.GetXattrName(dataBuff)
	if new_event.StorageArgs == nil {
		new_event.StorageArgs = &estorage.StorageArgs{}
	}
	new_event.StorageArgs.XAttrName = name

	if estorage.HasFlagXAttr(context.Etype) {
		flags := int(int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))))
		flag_str := estorage.GetXAttrFlags(flags)
		new_event.StorageArgs.Flags = flag_str
	}

	return new_event, nil
}

func (tracer *Tracer) ParseReadaheadEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}
	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.StorageArgs = &estorage.StorageArgs{FileDescriptor: &file_descriptor}
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}

	offset := int64(bcc.GetHostByteOrder().Uint64(dataBuff.Next(8)))
	new_event.StorageArgs.Offset = &offset
	count := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	new_event.StorageArgs.Count = &count
	return new_event, nil
}

func (tracer *Tracer) ParseLSeekEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*estorage.StorageEvent, error) {
	new_event := &estorage.StorageEvent{}
	file_descriptor := int32(bcc.GetHostByteOrder().Uint32(dataBuff.Next(4)))
	new_event.StorageArgs = &estorage.StorageArgs{FileDescriptor: &file_descriptor}
	dev := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	ino := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	iTimestamp := bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	if *context.ReturnValue >= 0 {
		new_event.FileTag = fmt.Sprintf("%v|%v|%v", dev, ino, iTimestamp)
	}

	offset := int64(bcc.GetHostByteOrder().Uint64(dataBuff.Next(8)))
	new_event.StorageArgs.Offset = &offset
	whence := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	new_event.StorageArgs.Whence = estorage.GetSeekWhence(whence)
	return new_event, nil
}

func (tracer *Tracer) ParseProcessCreateEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*eprocess.ProcessEvent, error) {
	new_event := &eprocess.ProcessEvent{}

	child_pid := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	new_event.Child = &child_pid

	context.ReturnTimestamp = nil
	context.ExecutionTime = nil
	context.TimeReturned = ""
	context.ReturnValue = nil

	return new_event, nil
}

func (tracer *Tracer) ParseProcessEndEvent(dataBuff *bytes.Buffer, context *event.EventContext) (*eprocess.ProcessEvent, error) {
	new_event := &eprocess.ProcessEvent{}

	if tracer.gotAllEndEvents == false && !tracer.conf.TraceAllProcesses {
		if len(tracer.conf.TargetPids) > 0 {
			tracer.conf.TargetPids = utils.RemoveElemFromSlice(tracer.conf.TargetPids, context.Tid)
		}
		if len(tracer.conf.TargetTids) > 0 {
			tracer.conf.TargetTids = utils.RemoveElemFromSlice(tracer.conf.TargetTids, context.Tid)
		}
		if len(tracer.conf.TargetPids) == 0 && len(tracer.conf.TargetTids) == 0 {
			tracer.gotAllEndEvents = true
		}
	}
	context.ReturnTimestamp = nil
	context.ExecutionTime = nil
	context.TimeReturned = ""
	return new_event, nil
}

func (tracer *Tracer) ParseBaseEvent(dataBuff *bytes.Buffer) (*ebase.BaseEvent, error) {
	return &ebase.BaseEvent{}, nil
}

// ----

func (tracer *Tracer) ParseDataEvent(dataBuff *bytes.Buffer, eventID uint32, size int64, cpu uint16) (*event.EventContent, int64, error) {
	content := &event.EventContent{}

	content.BytesRequest = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
	offset := int64(bcc.GetHostByteOrder().Uint64(dataBuff.Next(8)))
	mark_as_incomplete := false

	if tracer.conf.DetailedWithContent == "kernel_hash" || tracer.conf.DetailedWithContent == "userspace_hash" || tracer.conf.DetailedWithContent == "plain" {
		content.CapturedSize = bcc.GetHostByteOrder().Uint64(dataBuff.Next(8))
		if int64(content.CapturedSize) < size {
			if int64(content.CapturedSize) < ebpfs.MAX_BUF_SIZE {
				mark_as_incomplete = true
			} else {
				tracer.stats.UpdateTruncateEvents(eventID)
			}
		}
	}
	if tracer.conf.DetailedWithContent == "kernel_hash" {
		hash := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
		if hash == 0 {
			content.Signature = ""
			content.CapturedSize = 0
			mark_as_incomplete = true
			utils.WarningLogger.Printf("failed to compute buffer hash. Length was %v. Event was %s\n", size, event.GetEventName(eventID))
		} else {
			content.Signature = fmt.Sprintf("%x", hash)
		}
	} else if tracer.conf.DetailedWithContent == "userspace_hash" || tracer.conf.DetailedWithContent == "plain" {
		msg, err := tracer.bpfConf.GetDataContent(dataBuff, content.CapturedSize, cpu)
		if err != nil {
			mark_as_incomplete = true
		}
		content.CapturedBuffer = msg

		if mark_as_incomplete {
			tracer.stats.UpdateIncompleteEvents(eventID)
		}
	}

	return content, offset, nil
}

func (tracer *Tracer) ParseContext(buf *bytes.Buffer, etype uint32, sessionId string) (*event.EventContext, error) {

	err := ebpfs.CheckEventContext(buf)
	if err != nil {
		return nil, err
	}

	context := &event.EventContext{}
	context.SessionName = sessionId
	context.Etype = etype

	context.Tid = bcc.GetHostByteOrder().Uint32(buf.Next(4))
	context.Pid = bcc.GetHostByteOrder().Uint32(buf.Next(4))
	context.Ppid = bcc.GetHostByteOrder().Uint32(buf.Next(4))

	call_timestamp_since_boot := bcc.GetHostByteOrder().Uint64(buf.Next(8))
	call_time := utils.Epoch2dateTime(call_timestamp_since_boot)
	context.CallTimestamp = uint64(call_time.UnixNano())
	context.TimeCalled = utils.DateTime2String(call_time)

	return_timestamp_since_boot := bcc.GetHostByteOrder().Uint64(buf.Next(8))
	return_time := utils.Epoch2dateTime(return_timestamp_since_boot)
	return_timestamp_since_epoch := uint64(return_time.UnixNano())
	context.ReturnTimestamp = &return_timestamp_since_epoch
	context.TimeReturned = utils.DateTime2String(return_time)

	exec_time := *context.ReturnTimestamp - context.CallTimestamp
	context.ExecutionTime = &exec_time

	if !tracer.min_set {
		tracer.min_t = context.CallTimestamp
		tracer.min_set = true
	} else if tracer.min_t > context.CallTimestamp {
		tracer.min_t = context.CallTimestamp
	}
	if tracer.max_t < *context.ReturnTimestamp {
		tracer.max_t = *context.ReturnTimestamp
	}

	return_value := int64(bcc.GetHostByteOrder().Uint64(buf.Next(8)))
	if return_value < 0 {
		context.Errno = utils.GetErrorMessage(return_value)
		return_value = -1
		context.ReturnValue = &return_value

	} else {
		context.ReturnValue = &return_value
	}

	context.Comm = tracer.bpfConf.GetCommStr(buf)

	context.CPU = bcc.GetHostByteOrder().Uint16(buf.Next(2))

	syscall, ok := event.SuportedEvents[context.Etype]
	if ok {
		context.Syscall = syscall.Name
		context.Category = syscall.Category
		context.EventType = syscall.EventType
		context.FuncType = syscall.FuncType
	}
	return context, nil
}

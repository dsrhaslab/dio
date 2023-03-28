package events

import (
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
)

type StorageArgs struct {
	FileDescriptor *int32  `json:"file_descriptor,omitempty"`
	Flags          string  `json:"flags,omitempty"`
	Mode           string  `json:"mode,omitempty"`
	XAttrName      string  `json:"name,omitempty"`
	TruncLength    *uint64 `json:"length,omitempty"`
	Dev            *uint16 `json:"dev,omitempty"`
	Offset         *int64  `json:"offset,omitempty"`
	NewName        string  `json:"newname,omitempty"`
	Buf            string  `json:"buf,omitempty"`
	Count          *uint64 `json:"count,omitempty"`
	Whence         string  `json:"whence,omitempty"`
	*event.EventContent
}

type StorageEvent struct {
	event.EventContext
	*StorageArgs      `json:"args,omitempty"`
	FileTag           string `json:"file_tag,omitempty"`
	*PathArg          `json:"fdata,omitempty"`
	*event.SocketData `json:"socket_data,omitempty"`
}

type PathArg struct {
	Filename string `json:"file_path,omitempty"`
}

func (ev *StorageEvent) GetType() uint32 {
	return ev.Etype
}
func (ev *StorageEvent) GetName() string {
	return event.GetEventName(ev.GetType())
}
func (ev *StorageEvent) SetContext(context *event.EventContext) {
	ev.EventContext = *context
}
func (ev *StorageEvent) ComputeHash() {
	if ev.StorageArgs != nil && ev.StorageArgs.EventContent != nil {
		ev.StorageArgs.EventContent.ComputeHash()
	}
}
func (ev *StorageEvent) GetTimes() (uint64, uint64) {
	return ev.CallTimestamp, *ev.ReturnTimestamp
}

func HasFileDescriptor(etype uint32) bool {
	switch etype {
	case event.DIO_FGETXATTR, event.DIO_FSETXATTR, event.DIO_FREMOVEXATTR, event.DIO_FLISTXATTR, event.DIO_FTRUNCATE:
		return true
	default:
		break
	}
	return false
}

func HasFlagXAttr(etype uint32) bool {
	if etype == event.DIO_SETXATTR || etype == event.DIO_FSETXATTR || etype == event.DIO_LSETXATTR {
		return true
	}
	return false
}

package events

import (
	"bytes"
	"fmt"

	"github.com/pierrec/xxHash/xxHash32"
)

type Event interface {
	GetType() uint32
	GetName() string
	SetContext(*EventContext)
	ComputeHash()
	GetTimes() (uint64, uint64)
}

type EventContext struct {
	Syscall         string  `json:"system_call_name,omitempty"`
	TimeCalled      string  `json:"time_called,omitempty"`
	TimeReturned    string  `json:"time_returned,omitempty"`
	CallTimestamp   uint64  `json:"call_timestamp"`
	ReturnTimestamp *uint64 `json:"return_timestamp,omitempty"`
	ExecutionTime   *uint64 `json:"execution_time,omitempty"`
	Etype           uint32  `json:"-"`
	Thread          string  `json:"thread,omitempty"`
	Pid             uint32  `json:"pid,omitempty"`
	Tid             uint32  `json:"tid,omitempty"`
	Ppid            uint32  `json:"ppid,omitempty"`
	ReturnValue     *int64  `json:"return_value,omitempty"`
	Errno           string  `json:"error_message,omitempty"`
	Category        string  `json:"category,omitempty"`
	EventType       string  `json:"event_type,omitempty"`
	FuncType        string  `json:"type,omitempty"`
	ExtraData       `json:",omitempty"`
	SessionName     string `json:"session_name,omitempty"`
	CPU             uint16 `json:"cpu"`
}

type EventContent struct {
	CapturedBuffer string `json:"captured_buffer,omitempty"`
	CapturedSize   uint64 `json:"captured_size,omitempty"`
	Signature      string `json:"signature,omitempty"`
	BytesRequest   uint64 `json:"bytes_requested"`
}

type ExtraData struct {
	Host string `json:"hostname,omitempty"`
	Comm string `json:"comm,omitempty"`
}

type SocketData struct {
	SocketFamily  string `json:"address_family,omitempty"`
	SocketAddress string `json:"socket_address,omitempty"`
}

func computeXXHash32(msg string) string {
	buf := bytes.NewBufferString(msg)
	hash := fmt.Sprintf("%x", xxHash32.Checksum(buf.Bytes(), 12345))
	return hash
}

func (ev *EventContent) ComputeHash() {
	ev.Signature = computeXXHash32(ev.CapturedBuffer)
	ev.CapturedBuffer = ""
}

func IsStorageEvent(etype uint32) bool {
	return SuportedEvents[etype].EventType == "storage"
}

func IsNetworkEvent(etype uint32) bool {
	return SuportedEvents[etype].EventType == "network"
}

func IsProcessEvent(etype uint32) bool {
	return SuportedEvents[etype].EventType == "process"
}

func IsPathEvent(etype uint32) bool {
	return SuportedEvents[etype].EventType == "path"
}

func GetEventName(etype uint32) string {
	return SuportedEvents[etype].Name
}

func GetEventType(name string) uint32 {
	return EventName2ID[name]
}

func GetEventInfo(name string) (EventInfo, bool) {
	info, ok := SuportedEvents[GetEventType(name)]
	return info, ok
}

func GetEventArgs(etype uint32) []Arg {
	return SuportedEvents[etype].Args
}

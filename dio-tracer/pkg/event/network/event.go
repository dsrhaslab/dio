package events

import (
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
)

type NetworkArgs struct {
	FileDescriptor       *int32  `json:"file_descriptor,omitempty"`
	Backlog              *int32  `json:"backlog,omitempty"`
	Family               string  `json:"addr_family,omitempty"`
	AddrLen              *int32  `json:"addr_len,omitempty"`
	InAddr               string  `json:"in_address,omitempty"`
	InPort               *uint16 `json:"in_port,omitempty"`
	UnPath               string  `json:"un_path,omitempty"`
	NlPid                *uint32 `json:"nl_pid,omitempty"`
	NlGroups             *uint32 `json:"nl_groups,omitempty"`
	SecondFileDescriptor *uint32 `json:"file_descriptor2,omitempty"`
	SocketType           string  `json:"socket_type,omitempty"`
	Protocol             string  `json:"protocol,omitempty"`
	Flags                string  `json:"flags,omitempty"`
	Level                string  `json:"level,omitempty"`
	OptName              string  `json:"optnamem,omitempty"`
	*event.EventContent
}

type NetworkEvent struct {
	event.EventContext
	*NetworkArgs      `json:"args,,omitempty"`
	*event.SocketData `json:"socket_data,omitempty"`
	FileTag           string `json:"file_tag,omitempty"`
}

func (ev *NetworkEvent) GetType() uint32 {
	return ev.Etype
}
func (ev *NetworkEvent) GetName() string {
	return event.GetEventName(ev.GetType())
}
func (ev *NetworkEvent) SetContext(context *event.EventContext) {
	ev.EventContext = *context
}
func (ev *NetworkEvent) ComputeHash() {
	if ev.NetworkArgs != nil && ev.NetworkArgs.EventContent != nil {
		ev.EventContent.ComputeHash()
	}
}
func (ev *NetworkEvent) GetTimes() (uint64, uint64) {
	return ev.CallTimestamp, *ev.ReturnTimestamp
}

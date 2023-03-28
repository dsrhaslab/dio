package events

import (
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
)

type EventPath struct {
	DocType   string `json:"doc_type"`
	SessionID string `json:"session_name,omitempty"`
	PathInfo
	FileTag string `json:"file_tag"`
}

type PathInfo struct {
	Filename string `json:"file_path"`
	FileType string `json:"file_type"`
}

func (ev *EventPath) GetType() uint32 {
	return event.DIO_PATH_EVENT
}
func (ev *EventPath) GetName() string {
	return event.GetEventName(ev.GetType())
}
func (ev *EventPath) SetContext(context *event.EventContext) {}
func (ev *EventPath) ComputeHash()                           {}
func (ev *EventPath) GetTimes() (uint64, uint64) {
	return 0, 0
}

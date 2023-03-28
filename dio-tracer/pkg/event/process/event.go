package events

import (
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
)

type ProcessEvent struct {
	event.EventContext
	Child *uint32 `json:"child,omitempty"`
}

func (ev *ProcessEvent) GetType() uint32 {
	return ev.Etype
}
func (ev *ProcessEvent) GetName() string {
	return event.GetEventName(ev.GetType())
}
func (ev *ProcessEvent) SetContext(context *event.EventContext) {
	ev.EventContext = *context
}
func (ev *ProcessEvent) ComputeHash() {}
func (ev *ProcessEvent) GetTimes() (uint64, uint64) {
	return ev.CallTimestamp, *ev.ReturnTimestamp
}

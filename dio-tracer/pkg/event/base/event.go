package events

import (
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
)

type BaseEvent struct {
	event.EventContext
}

func (ev *BaseEvent) GetType() uint32 {
	return ev.Etype
}
func (ev *BaseEvent) GetName() string {
	return event.GetEventName(ev.GetType())
}
func (ev *BaseEvent) SetContext(context *event.EventContext) {
	ev.EventContext = *context
}
func (ev *BaseEvent) ComputeHash() {}
func (ev *BaseEvent) GetTimes() (uint64, uint64) {
	return ev.CallTimestamp, *ev.ReturnTimestamp
}

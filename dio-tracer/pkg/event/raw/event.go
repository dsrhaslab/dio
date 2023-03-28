package events

import (
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
)

type EventRaw struct {
	event.EventContext
	Args map[string]interface{} `json:"args,omitempty"`
}

func (ev *EventRaw) GetType() uint32 {
	return ev.Etype
}
func (ev *EventRaw) GetName() string {
	return event.GetEventName(ev.GetType())
}
func (ev *EventRaw) SetContext(context *event.EventContext) {
	ev.EventContext = *context
}
func (ev *EventRaw) ComputeHash() {}
func (ev *EventRaw) GetTimes() (uint64, uint64) {
	return ev.CallTimestamp, *ev.ReturnTimestamp
}

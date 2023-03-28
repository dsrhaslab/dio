package storage

import (
	"sync"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/config"
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"
)

type Writer struct {
	conf         *config.NopWriterConf
	mutex        sync.RWMutex
	n_events     int
	saved_events int
}

func Open(conf *config.NopWriterConf, sessionID string) (*Writer, error) {
	return &Writer{
		conf:     conf,
		n_events: 0,
	}, nil
}

func (nwriter *Writer) Save(v event.Event) error {

	nwriter.mutex.Lock()
	nwriter.n_events += 1
	nwriter.mutex.Unlock()

	return nil
}

func (nwriter *Writer) PrintTotalEvents() {
	utils.InfoLogger.Printf("Writer received %d events\n", nwriter.n_events)
}

func (nwriter *Writer) Close(min, max uint64) error {
	return nil
}

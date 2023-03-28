package storage

import (
	"encoding/binary"
	"fmt"
	"os"
	"sync"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/config"
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"
)

type Writer struct {
	conf         *config.FileWriterConf
	out_file     *os.File
	mutex        sync.RWMutex
	n_events     int
	saved_events int
	last_off     int
}

func Open(conf *config.FileWriterConf, sessionID string) (*Writer, error) {
	out_file, err := os.OpenFile(conf.Filename, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0644)
	if err != nil {
		return nil, fmt.Errorf("Failed to open output file: %v", err)
	}

	return &Writer{
		conf:         conf,
		out_file:     out_file,
		n_events:     0,
		saved_events: 0,
		last_off:     0,
	}, nil
}

func (jwriter *Writer) Save(v event.Event) error {
	var m []byte

	jwriter.mutex.Lock()
	jwriter.n_events += 1
	jwriter.mutex.Unlock()

	m, err := utils.JSONMarshal(v)
	if utils.LoggerConf.SaveTimestamps {
		utils.SaveTimestamp("add2disk", 1, binary.Size(m))
	}
	if err != nil {
		return fmt.Errorf("error marshalling json data: %v", v)
	}
	_, err = jwriter.out_file.Write(m)
	if err != nil {
		return fmt.Errorf("failed saving event to file: %s", err.Error())
	}

	jwriter.mutex.Lock()
	jwriter.saved_events += 1
	jwriter.mutex.Unlock()

	return nil
}

func (jwriter *Writer) PrintTotalEvents() {
	utils.InfoLogger.Printf("Writer received %d events and save %d to file '%s'\n", jwriter.n_events, jwriter.saved_events, jwriter.conf.Filename)
}

func (jwriter *Writer) Close(min, max uint64) error {
	jwriter.mutex.Lock()
	defer jwriter.mutex.Unlock()

	if utils.LoggerConf.SaveTimestamps {
		utils.SaveTimestamp("beforeSync2disk", 1, 0)
	}

	utils.ProfilingStartMeasurement("file_writr_sync")
	jwriter.out_file.Sync()
	utils.ProfilingStopMeasurement("file_writr_sync")
	if utils.LoggerConf.SaveTimestamps {
		utils.SaveTimestamp("afterSync2disk", 1, 0)
	}

	jwriter.out_file.Close()

	return nil
}

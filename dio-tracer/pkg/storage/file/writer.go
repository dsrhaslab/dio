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
	first        bool
	last_off     int
	bulk         []event.Event
	bulkSize     int
}

func Open(conf *config.FileWriterConf, sessionID string) (*Writer, error) {
	out_file, err := os.OpenFile(conf.Filename, os.O_CREATE|os.O_WRONLY|os.O_TRUNC|os.O_APPEND, 0644)
	if err != nil {
		return nil, fmt.Errorf("failed to open output file: %v", err)
	}
	out_file.Write([]byte("["))

	return &Writer{
		conf:         conf,
		out_file:     out_file,
		n_events:     0,
		saved_events: 0,
		first:        true,
		last_off:     0,
		bulk:         []event.Event{},
		bulkSize:     0,
	}, nil
}

func (jwriter *Writer) Save(v event.Event) error {
	jwriter.mutex.Lock()
	jwriter.n_events += 1
	jwriter.mutex.Unlock()

	if jwriter.conf.Bulk {
		if jwriter.bulkSize >= jwriter.conf.BulkSize {
			jwriter.persistBulk()
		}
		jwriter.add2Bulk(v)
	} else {
		jwriter.persist(v)
	}

	return nil
}

func (jwriter *Writer) persist(v event.Event) error {
	m, err := utils.JSONMarshal(v)
	if err != nil {
		return fmt.Errorf("error marshalling json data: %v", v)
	}

	m = m[:len(m)-1]
	second := false
	jwriter.mutex.Lock()
	if jwriter.first {
		jwriter.first = false
		second = false
	} else {
		second = true
	}
	jwriter.mutex.Unlock()
	if second {
		bcomma := []byte(",\n")
		m = append(bcomma, m...)
	}
	if utils.LoggerConf.SaveTimestamps {
		utils.SaveTimestamp("add2disk", 1, binary.Size(m))
	}

	jwriter.first = true
	jwriter.mutex.Unlock()
	_, err = jwriter.out_file.Write(m)
	if err != nil {
		return fmt.Errorf("failed saving event to file: %s", err.Error())
	}

	jwriter.mutex.Lock()
	jwriter.saved_events += 1
	jwriter.mutex.Unlock()

	return err
}

func (jwriter *Writer) add2Bulk(v event.Event) {
	jwriter.mutex.Lock()
	jwriter.bulk = append(jwriter.bulk, v)
	jwriter.bulkSize += 1
	jwriter.mutex.Unlock()
}

func (jwriter *Writer) persistBulk() error {
	jwriter.mutex.Lock()
	bulk := jwriter.bulk
	bulkSize := jwriter.bulkSize
	jwriter.bulk = nil
	jwriter.bulkSize = 0
	jwriter.mutex.Unlock()

	var m []byte
	m, err := utils.JSONMarshal(bulk)
	if err != nil {
		return fmt.Errorf("error marshalling json data: %v", bulk)
	}

	m = m[1 : len(m)-2]
	second := false
	jwriter.mutex.Lock()
	if jwriter.first {
		jwriter.first = false
		second = false
	} else {
		second = true
	}
	jwriter.mutex.Unlock()
	if second {
		bcomma := []byte(",\n")
		m = append(bcomma, m...)
	}

	if utils.LoggerConf.SaveTimestamps {
		utils.SaveTimestamp("add2disk", 1, binary.Size(m))
	}
	_, err = jwriter.out_file.Write(m)
	if err != nil {
		return fmt.Errorf("failed saving event to file: %s", err.Error())
	}
	jwriter.mutex.Lock()
	jwriter.saved_events += bulkSize
	jwriter.mutex.Unlock()

	return err
}

func (jwriter *Writer) PrintTotalEvents() {
	utils.InfoLogger.Printf("Writer received %d events and save %d to file '%s'\n", jwriter.n_events, jwriter.saved_events, jwriter.conf.Filename)
}

func (jwriter *Writer) Close(min, max uint64) error {

	if jwriter.conf.Bulk && jwriter.bulkSize > 0 {
		jwriter.persistBulk()
	}

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

	jwriter.out_file.Write([]byte("]"))

	jwriter.out_file.Close()

	return nil
}

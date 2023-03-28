package storage

import (
	"fmt"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/config"
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/stats"
	elasticsearch "github.com/dsrhaslab/dio/dio-tracer/pkg/storage/elasticsearch"
	file "github.com/dsrhaslab/dio/dio-tracer/pkg/storage/file"
	nop "github.com/dsrhaslab/dio/dio-tracer/pkg/storage/nop"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"
)

type Writer interface {
	Save(v event.Event) error
	PrintTotalEvents()
	Close(min, max uint64) error
}

type Writers []Writer

func CreateStorageInstances(conf *config.OutputConf, sessionID string) (Writers, error) {
	writers := Writers{}
	enabled := 0

	if conf.NopWriterConf.Enabled {
		enabled += 1
		newWriter, err := nop.Open(&conf.NopWriterConf, sessionID)
		if err != nil {
			utils.ErrorLogger.Printf("failed to init FileWriter: %v\n", err)
		} else {
			writers = append(writers, newWriter)
		}
	}
	if conf.FileWriterConf.Enabled {
		enabled += 1
		newWriter, err := file.Open(&conf.FileWriterConf, sessionID)
		if err != nil {
			utils.ErrorLogger.Printf("failed to init FileWriter: %v\n", err)
		} else {
			writers = append(writers, newWriter)
		}
	}
	if conf.ElasticsearchWriterConf.Enabled {
		enabled += 1
		newWriter, err := elasticsearch.Open(&conf.ElasticsearchWriterConf, sessionID)
		if err != nil {
			utils.ErrorLogger.Printf("failed to init ElasticsearchWriter: %v\n", err)
		} else {
			writers = append(writers, newWriter)
		}
	}

	if len(writers) < enabled {
		return writers, fmt.Errorf("Some enabled writers failed to init")
	}
	return writers, nil
}

func (writers Writers) Save(event event.Event, stats *stats.Stats) error {
	saved := 0
	if len(writers) <= 0 {
		return nil
	}
	for _, writer := range writers {
		err := writer.Save(event)
		if err != nil {
			utils.ErrorLogger.Printf("failed to save event: %v", err)
		} else {
			saved += 1
		}
	}
	if saved == 0 {
		return fmt.Errorf("All writers failed to save event")
	}

	return nil
}

func (writers Writers) PrintTotalEvents() {
	if len(writers) <= 0 {
		return
	}
	for _, writer := range writers {
		writer.PrintTotalEvents()
	}
}

func (writers Writers) Close(min, max uint64) {
	if len(writers) <= 0 {
		return
	}
	for _, writer := range writers {
		err := writer.Close(min, max)
		if err != nil {
			utils.ErrorLogger.Printf("failed to close writer: %v", err)
		}
	}
}

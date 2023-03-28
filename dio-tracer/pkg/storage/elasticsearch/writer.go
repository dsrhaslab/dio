package storage

import (
	"bytes"
	"context"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/config"
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/stats"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"

	elasticsearch "github.com/elastic/go-elasticsearch/v7"
	esapi "github.com/elastic/go-elasticsearch/v7/esapi"
	"github.com/elastic/go-elasticsearch/v7/esutil"
)

var indexFilePath string = "/tmp/dio/dio_index"
var splitEventsPipeline = string(`{
	"description": "Split system calls into different indexes",
	"processors": [
		{
			"set": {
				"field": "_index",
				"value": "{{{ _index }}}-paths",
				"if": "if ( ctx.doc_type == \"EventPath\") { return true; }",
				"ignore_failure": true
			}
		}
	]
}`)

type Writer struct {
	es_client    *elasticsearch.Client
	ctx          context.Context
	session_name string
	index_name   string
	bi           esutil.BulkIndexer
	conf         *config.ElasticsearchWriterConf
}

type TimeEvent struct {
	SessionName string `json:"session_name"`
	Min         string `json:"min_t"`
	Max         string `json:"max_t"`
	Duration    int64  `json:"duration"`
}

func Open(conf *config.ElasticsearchWriterConf, sessionName string) (*Writer, error) {
	var err error

	utils.ComputeBootTime()

	// Create a context object for the API calls
	ctx := context.Background()

	utils.InfoLogger.Printf("Initializing ElasticsearchWriter. Elasticsearch servers are: %v\n", conf.Servers)
	// Create client Config
	cfg := elasticsearch.Config{
		Addresses:            conf.Servers,
		MaxRetries:           10,
		EnableRetryOnTimeout: true,
		Username:             conf.Username,
		Password:             conf.Password,
	}

	// Create es client
	es, err := elasticsearch.NewClient(cfg)
	if err != nil {
		return nil, fmt.Errorf("Failed to create elasticsearch client %v", err)
	}

	// Create indexes
	index_name := "dio_trace_" + strings.ToLower(sessionName)

	mappings := string(`{
		"mappings": {
		  "properties": {
			"time_called": { "type": "date_nanos" },
			"time_returned": { "type": "date_nanos" }
		  }
		}
	  }`)
	err = createIndex(es, index_name, mappings)
	if err != nil {
		return nil, fmt.Errorf("failed to create index %s: %v", index_name, err)
	}
	err = createIndex(es, index_name+"-paths", "")
	if err != nil {
		return nil, fmt.Errorf("failed to create index %s: %v", index_name+"-paths", err)
	}

	// Create ingest pipeline
	splitEventsIngestPipelineName := "split-events-pipeline"
	err = createIngestPipeline(es, splitEventsIngestPipelineName, splitEventsPipeline)
	if err != nil {
		return nil, fmt.Errorf("failed to create ingest pipeline %s: %v", splitEventsIngestPipelineName, err)
	}

	// Create the BulkIndexerConfig
	biConf := esutil.BulkIndexerConfig{
		Index:         index_name,                                      // The default index name
		Client:        es,                                              // The Elasticsearch client
		NumWorkers:    4,                                               // The number of worker goroutines
		FlushBytes:    conf.FlushBytes,                                 // The flush threshold in bytes
		FlushInterval: time.Duration(conf.FlushInterval) * time.Second, // The periodic flush interval
		Pipeline:      splitEventsIngestPipelineName,
	}

	// Create the BulkIndexer
	var bi esutil.BulkIndexer
	if conf.FlushBytes > 0 || conf.FlushInterval > 0 {
		bi, err = esutil.NewBulkIndexer(biConf)
		if err != nil {
			return nil, fmt.Errorf("Failed to create bulk indexer: %s", err)
		}
	} else {
		bi = nil
	}

	saveIndex2File(sessionName)

	return &Writer{
		conf:         conf,
		es_client:    es,
		ctx:          ctx,
		session_name: sessionName,
		index_name:   index_name,
		bi:           bi,
	}, nil
}

func (esWriter *Writer) Save(v event.Event) error {

	var err error = nil

	// send single event
	if esWriter.bi == nil {
		err = esWriter.indexSingleEvent(v)
	} else { // send batch
		err = esWriter.add2BulkIndexer(v)
	}

	return err
}

func (esWriter *Writer) PrintTotalEvents() {
	if esWriter.bi != nil {
		biStats := esWriter.bi.Stats()
		utils.InfoLogger.Printf("esWriter sent %v events to Elasticsearch (NumRequests: %v, Errors: %v)\n", biStats.NumFlushed, biStats.NumRequests, biStats.NumFailed)
		stats.UpdateESFlushed(biStats.NumFlushed)
	}
}

func (esWriter *Writer) Close(min, max uint64) error {

	// send min, max and duration values
	min_t := utils.Nanosecond2Time(min)
	max_t := utils.Nanosecond2Time(max)
	diff := max_t.Sub(min_t)
	duration := int64(diff / time.Nanosecond)

	times := &TimeEvent{
		SessionName: esWriter.session_name,
		Min:         utils.DateTime2String(min_t),
		Max:         utils.DateTime2String(max_t),
		Duration:    duration,
	}
	esWriter.indexSingleEvent(times)

	if esWriter.bi != nil {
		if err := esWriter.bi.Close(esWriter.ctx); err != nil {
			return fmt.Errorf("failed to close bulkIndexer: %v", err)
		}
	}

	removeIndex2File()

	return nil
}

func (esWriter *Writer) indexSingleEvent(v interface{}) error {

	if utils.LoggerConf.SaveTimestamps {
		utils.SaveTimestamp("indexSingleEvent", 1, binary.Size(v))
	}
	dataJSON, err := json.Marshal(v)
	if err != nil {
		panic(err)
	}
	js := string(dataJSON)

	// Instantiate a request object
	req := esapi.IndexRequest{
		Index:    esWriter.index_name,
		Body:     strings.NewReader(js),
		Pipeline: "split-events-pipeline",
	}

	// Send event to Elasticsearch
	res, err := req.Do(esWriter.ctx, esWriter.es_client)
	if err != nil {
		return fmt.Errorf("failed sending event to Elasticsearch: %s", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		return fmt.Errorf("%s error indexing document", res.Status())
	}
	return nil
}

func (esWriter *Writer) add2BulkIndexer(v interface{}) error {

	dataJSON, err := json.Marshal(v)
	if utils.LoggerConf.SaveTimestamps {
		utils.SaveTimestamp("add2BulkIndexer", 1, binary.Size(dataJSON))
	}
	if err != nil {
		return fmt.Errorf("failed to marshal event: %v", err)
	}

	err = esWriter.bi.Add(
		esWriter.ctx,
		esutil.BulkIndexerItem{
			Action: "index",
			Body:   bytes.NewReader(dataJSON),
			OnFailure: func(ctx context.Context, item esutil.BulkIndexerItem, res esutil.BulkIndexerResponseItem, err error) {
				if err != nil {
					utils.ErrorLogger.Printf("BulkIndexer error: %s\n", err)
				} else {
					utils.ErrorLogger.Printf("BulkIndexer failed: %s: %s\n", res.Error.Type, res.Error.Reason)
					utils.SaveTimestamp("bulk_index_error", 1, binary.Size(v))
				}
			},
		},
	)
	if err != nil {
		return fmt.Errorf("unexpected error: %s\n", err)
	}
	return nil
}

func createIndex(es_client *elasticsearch.Client, sessionID string, mappings string) error {
	var res *esapi.Response
	var err error
	if mappings != "" {
		res, err = es_client.Indices.Create(sessionID, es_client.Indices.Create.WithBody(strings.NewReader(mappings)))
	} else {
		res, err = es_client.Indices.Create(sessionID)
	}
	if err != nil {
		return err
	}

	var r map[string]interface{}
	if err := json.NewDecoder(res.Body).Decode(&r); err != nil {
		return fmt.Errorf("failed to parse the response body: %s", err)
	}

	if res.StatusCode != 200 {
		var errorstr map[string]interface{} = r["error"].(map[string]interface{})
		return fmt.Errorf("%s: %s", errorstr["type"].(string), errorstr["reason"].(string))
	}

	return nil
}

func createIngestPipeline(es_client *elasticsearch.Client, pipelineName, pipelineBody string) error {
	res, err := es_client.Ingest.PutPipeline(pipelineName, strings.NewReader(pipelineBody))
	if err != nil {
		return err
	}

	var r map[string]interface{}
	if err := json.NewDecoder(res.Body).Decode(&r); err != nil {
		return fmt.Errorf("failed to parse the response body: %s", err)
	}

	if res.StatusCode != 200 {
		var errorstr map[string]interface{} = r["error"].(map[string]interface{})
		return fmt.Errorf("%s: %s", errorstr["type"].(string), errorstr["reason"].(string))
	}

	return nil
}

func saveIndex2File(session_name string) error {
	file, err := os.OpenFile(indexFilePath, os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0666)
	if err != nil {
		return fmt.Errorf("failed to open file %s: %v", indexFilePath, err)
	}
	file.WriteString(session_name)
	file.Close()
	return nil
}

func removeIndex2File() error {
	err := os.Remove(indexFilePath)
	if err != nil {
		return fmt.Errorf("failed to remove file %s: %v", indexFilePath, err)
	}
	return nil
}

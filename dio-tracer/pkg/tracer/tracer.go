package tracer

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"sync"
	"time"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/config"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/ebpfs"
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/stats"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/storage"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"

	"github.com/iovisor/gobpf/bcc"
)

type Tracer struct {
	bpfConf         *ebpfs.BpfConf
	conf            *config.TracerConf
	writers         storage.Writers
	stats           *stats.Stats
	consumerChan    chan *event.Event
	consumersWG     sync.WaitGroup
	eventChan       chan []byte
	handlersWG      sync.WaitGroup
	StopChan        chan bool
	finishedChan    chan bool
	host            string
	progExit        bool
	gotAllEndEvents bool
	min_t           uint64
	max_t           uint64
	min_set         bool
	trace_by_pid    bool
}

func InitTracer(conf *config.TConfiguration, trace_by_pid bool) (*Tracer, error) {
	utils.ProfilingStartMeasurement("total_execution")
	utils.ProfilingStartMeasurement("tracer_init")

	utils.ProfilingStartMeasurement("prepare_bpf_program")
	bpfConf, err := ebpfs.PrepareBpf(conf)
	if err != nil {
		return nil, err
	}
	utils.ProfilingStopMeasurement("prepare_bpf_program")
	utils.ProfilingStartMeasurement("attaching_probes")
	err = bpfConf.AttachProbes(conf.Events, !conf.DetailedData)
	if err != nil {
		return nil, fmt.Errorf("failed to attach probes: %s", err)
	}
	utils.ProfilingStopMeasurement("attaching_probes")

	utils.ProfilingStartMeasurement("set_filter_lists")
	// Setup Pids to filter
	err = bpfConf.SetupPidList(conf.TargetPids)
	if err != nil {
		return nil, fmt.Errorf("failed to setup PID list: %s", err)
	}

	// Setup Tids to filter
	err = bpfConf.SetupTidList(conf.TargetTids)
	if err != nil {
		return nil, fmt.Errorf("failed to setup TID list: %s", err)
	}

	// Setup Paths to filter
	err = bpfConf.SetupTargetPaths(conf.TargetPaths)
	if err != nil {
		return nil, fmt.Errorf("failed to setup list of target paths: %s", err)
	}
	utils.ProfilingStopMeasurement("set_filter_lists")

	utils.ProfilingStartMeasurement("set_host_and_session_id")
	host, err := utils.GetHostName()
	if err != nil {
		return nil, err
	}

	if conf.TracerConf.SessionName == "" {
		conf.TracerConf.SessionName = utils.GenerateSessionID()
	}
	utils.ProfilingStopMeasurement("set_host_and_session_id")

	tracer := &Tracer{
		bpfConf:         bpfConf,
		conf:            &conf.TracerConf,
		stats:           &stats.Stats{},
		consumerChan:    make(chan *event.Event, 100000),
		eventChan:       make(chan []byte, 100000),
		StopChan:        make(chan bool, 1),
		finishedChan:    make(chan bool, 1),
		host:            host,
		gotAllEndEvents: false,
		progExit:        false,
		trace_by_pid:    trace_by_pid,
	}

	tracer.stats.SessionID = tracer.conf.SessionName
	utils.ProfilingStartMeasurement("init_writers")
	writers, err := storage.CreateStorageInstances(&conf.OutputConf, tracer.conf.SessionName)
	if err != nil {
		return nil, err
	}
	tracer.writers = writers
	utils.ProfilingStopMeasurement("init_writers")

	utils.ProfilingStopMeasurement("tracer_init")
	return tracer, nil
}

func (tracer *Tracer) Run() error {
	utils.ProfilingStartMeasurement("tracer_execution")
	// prepare bpf tables
	err := tracer.bpfConf.OpenBpfTables(tracer.eventChan)
	if err != nil {
		return err
	}

	// start consumers
	utils.ProfilingStartMeasurement("consumers_execution")
	tracer.consumersWG.Add(tracer.conf.Consumers)
	for i := 0; i < tracer.conf.Consumers; i++ {
		go tracer.runConsumer()
	}

	// start handlers
	tracer.handlersWG.Add(1)
	utils.ProfilingStartMeasurement("handlers_execution")
	go tracer.handleEvents()
	go tracer.handleStopTracing()

	// start pooling
	tracer.bpfConf.StartPooling()

	utils.InfoLogger.Printf("Starting tracing... Session name is '%v'\n", tracer.conf.SessionName)
	return nil
}

func (tracer *Tracer) Close() {

	// Stop pooling and close the channels
	<-tracer.finishedChan
	if tracer.progExit {
		utils.ProfilingStopMeasurement("pooling_remaing_events")
	}
	tracer.bpfConf.StopPooling()
	close(tracer.eventChan)
	tracer.handlersWG.Wait()
	utils.ProfilingStopMeasurement("handlers_execution")
	close(tracer.consumerChan)
	tracer.consumersWG.Wait()
	utils.ProfilingStopMeasurement("consumers_execution")

	// Print stats
	utils.ProfilingStartMeasurement("print_stats")
	if tracer.conf.Stats {
		tracer.stats.PrintStats(tracer.bpfConf, tracer.conf.StatsFile)
	}
	utils.ProfilingStopMeasurement("print_stats")
	if utils.LoggerConf.SaveTimestamps {
		calls_times, submitted_times, lost_times, err := tracer.bpfConf.GetTimesLogs()
		if err != nil {
			utils.ErrorLogger.Printf("Failed to get log timestamps from kernel: %s\n", err)
		}
		utils.SaveKernelTimestamps(calls_times, submitted_times, lost_times)
	}

	// Close bpfConf and writers
	utils.ProfilingStartMeasurement("close_bpfconf")
	tracer.bpfConf.Close()
	utils.ProfilingStopMeasurement("close_bpfconf")
	utils.ProfilingStartMeasurement("close_writers")
	tracer.writers.Close(tracer.min_t, tracer.max_t)
	tracer.writers.PrintTotalEvents()
	utils.ProfilingStopMeasurement("close_writers")

	utils.ProfilingStopMeasurement("tracer_execution")
	utils.ProfilingStopMeasurement("total_execution")
	utils.CloseLogger(tracer.conf.SessionName)
}

// ----

func (tracer *Tracer) runConsumer() {
	savedStats := stats.SavedStats{}
	for ev := range tracer.consumerChan {
		if tracer.conf.DetailedWithContent == "userspace_hash" {
			(*ev).ComputeHash()
		}
		err := tracer.writers.Save(*ev, tracer.stats)
		if err != nil && utils.LoggerConf.DebugMode {
			utils.DebugLogger.Printf("Failed to save event: %v\n", err)
		} else {
			savedStats.UpdateSaved((*ev).GetType())
		}
	}
	tracer.stats.SaveStats(savedStats)
	tracer.consumersWG.Done()
}

func (tracer *Tracer) handleEvents() {
	waiting_time := 0
L:
	for {
		select {
		case dataRaw, ok := <-tracer.eventChan:
			if ok {
				if utils.LoggerConf.SaveTimestamps {
					utils.SaveTimestamp("readEventFromRingBuffer", 1, binary.Size(dataRaw))
				}
				err := tracer.processRawEvent(&dataRaw)
				if err != nil && utils.LoggerConf.DebugMode {
					utils.DebugLogger.Printf(err.Error())
				}
				waiting_time = 0
			} else {
				break L
			}
		case <-time.After(2 * time.Second):
			waiting_time += 2
			if tracer.gotAllEndEvents {
				utils.InfoLogger.Println("All target pids/threads exited. Stopping now...")
				tracer.finishedChan <- true
				break L
			} else if tracer.progExit {
				utils.InfoLogger.Println("No longer receiving event. Stopping now...")
				tracer.finishedChan <- true
				break L
			} else if tracer.conf.Timeout != -1 && (waiting_time == tracer.conf.Timeout) {
				utils.InfoLogger.Printf("Timeout! No events received in the last %v seconds. Stopping now...\n", tracer.conf.Timeout)
				tracer.finishedChan <- true
				break L
			}
		}
	}
	tracer.handlersWG.Done()
}

func (tracer *Tracer) handleStopTracing() {
	var timeout_seconds time.Duration
	<-tracer.StopChan
	tracer.progExit = true
	utils.ProfilingStartMeasurement("pooling_remaing_events")
	if tracer.conf.TraceAllProcesses {
		tracer.gotAllEndEvents = true
		tracer.finishedChan <- true
	}
	if tracer.conf.WaitTimeout != -1 {
		timeout_seconds = time.Duration(tracer.conf.WaitTimeout) * time.Second
		utils.InfoLogger.Printf("Waiting %v before stopping the tracer...\n", timeout_seconds)
		time.Sleep(timeout_seconds)
		utils.InfoLogger.Printf("Timeout! Stopping the tracer now!\n")
		tracer.finishedChan <- true
	} else {
		utils.InfoLogger.Printf("Waiting for all events to be processed before stopping tracer...\n")
	}
}

func (tracer *Tracer) processRawEvent(dataRaw *[]byte) error {
	dataBuff := bytes.NewBuffer(*dataRaw)
	var err error
	var context *event.EventContext
	event_type := bcc.GetHostByteOrder().Uint32(dataBuff.Next(4))
	if event_type != event.DIO_PATH_EVENT {
		context, err = tracer.ParseContext(dataBuff, event_type, tracer.conf.SessionName)
		if err != nil {
			return err
		}
		context.Host = tracer.host
		context.Thread = fmt.Sprintf("%d@%s", context.Tid, tracer.host)
	}

	tracer.stats.UpdateHandledEvents(event_type)

	var new_event event.Event
	if event_type == event.DIO_PROCESS_END {
		new_event, err = tracer.ParseProcessEndEvent(dataBuff, context)
		if !tracer.conf.CaptureProcessEvents {
			return nil
		}
	} else if event_type == event.DIO_PROCESS_FORK {
		new_event, err = tracer.ParseProcessCreateEvent(dataBuff, context)
	} else if tracer.conf.DetailedData {
		switch event_type {
		case event.DIO_PATH_EVENT:
			new_event, err = tracer.ParseEventPath(dataBuff)
		case event.DIO_OPEN, event.DIO_OPENAT, event.DIO_CREAT:
			new_event, err = tracer.ParseStorageOpenEvent(dataBuff, context)
		case event.DIO_READ, event.DIO_PREAD64, event.DIO_WRITE, event.DIO_PWRITE64, event.DIO_READV, event.DIO_WRITEV:
			new_event, err = tracer.ParseStorageDataEvent(dataBuff, context)
		case event.DIO_CLOSE, event.DIO_FSYNC, event.DIO_FDATASYNC, event.DIO_FSTAT,
			event.DIO_FSTATFS, event.DIO_FLISTXATTR:
			new_event, err = tracer.ParseFDEvent(dataBuff, context)
		case event.DIO_SOCKET, event.DIO_SOCKETPAIR:
			new_event, err = tracer.ParseNetworkSocketEvent(dataBuff, context)
		case event.DIO_BIND:
			new_event, err = tracer.ParseNetworkBindEvent(dataBuff, context)
		case event.DIO_LISTEN:
			new_event, err = tracer.ParseNetworkListenEvent(dataBuff, context)
		case event.DIO_CONNECT, event.DIO_ACCEPT, event.DIO_ACCEPT4:
			new_event, err = tracer.ParseNetworkConnectAcceptEvent(dataBuff, context)
		case event.DIO_RECVFROM, event.DIO_SENDTO, event.DIO_RECVMSG, event.DIO_SENDMSG:
			new_event, err = tracer.ParseNetworkDataEvent(dataBuff, context)
		case event.DIO_GETSOCKOPT, event.DIO_SETSOCKOPT:
			new_event, err = tracer.ParseNetworkSockOptEvent(dataBuff, context)
		case event.DIO_RENAME, event.DIO_RENAMEAT, event.DIO_RENAMEAT2:
			new_event, err = tracer.ParseRenameEvent(dataBuff, context)
		case event.DIO_READLINK, event.DIO_READLINKAT:
			new_event, err = tracer.ParseReadlinkEvent(dataBuff, context)
		case event.DIO_TRUNCATE, event.DIO_FTRUNCATE:
			new_event, err = tracer.ParseTruncateEvent(dataBuff, context)
		case event.DIO_UNLINK, event.DIO_UNLINKAT,
			event.DIO_STAT, event.DIO_LSTAT, event.DIO_FSTATAT,
			event.DIO_LISTXATTR, event.DIO_LLISTXATTR:
			new_event, err = tracer.ParseBasePathEvent(dataBuff, context)
		case event.DIO_MKNOD, event.DIO_MKNODAT:
			new_event, err = tracer.ParseMknodEvent(dataBuff, context)
		case event.DIO_GETXATTR, event.DIO_LGETXATTR, event.DIO_FGETXATTR,
			event.DIO_SETXATTR, event.DIO_LSETXATTR, event.DIO_FSETXATTR,
			event.DIO_REMOVEXATTR, event.DIO_LREMOVEXATTR, event.DIO_FREMOVEXATTR:
			new_event, err = tracer.ParseXAttrEvent(dataBuff, context)
		case event.DIO_READAHEAD:
			new_event, err = tracer.ParseReadaheadEvent(dataBuff, context)
		case event.DIO_LSEEK:
			new_event, err = tracer.ParseLSeekEvent(dataBuff, context)
		default:
			new_event, err = tracer.ParseBaseEvent(dataBuff)
		}
	} else {
		new_event, err = tracer.ParseEventRaw(dataBuff, context)
	}
	if err != nil {
		return fmt.Errorf("failed to parse %s: %v", utils.GetStructType(new_event), err)
	}

	if event_type != event.DIO_PATH_EVENT {
		new_event.SetContext(context)
	}
	tracer.consumerChan <- &new_event

	return nil
}

package ebpfs

/*
#cgo CFLAGS: -I/usr/include/bcc/compat
#cgo LDFLAGS: -lbcc
#include <bcc/bcc_common.h>
#include <bcc/libbpf.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "resources/bpfprogram.h"
*/
import "C"
import (
	"bytes"
	"encoding/binary"
	"fmt"
	"runtime"
	"strconv"
	"strings"
	"unsafe"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/config"
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
	path "github.com/dsrhaslab/dio/dio-tracer/pkg/event/path"
	estorage "github.com/dsrhaslab/dio/dio-tracer/pkg/event/storage"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"

	"embed"

	"github.com/iovisor/gobpf/bcc"
)

//go:embed resources/*
var resources embed.FS

const (
	TASK_COMM_LEN      int   = C.TASK_COMM_LEN
	MAX_BUF_SIZE       int64 = C.MAX_BUF_SIZE
	MY_UNIX_PATH_MAX   int   = C.MY_UNIX_PATH_MAX
	FILENAME_MAX       int   = C.FILENAME_MAX
	MAX_ATTR_NAME_SIZE int   = C.MAX_ATTR_NAME_SIZE
	MAX_FILE_OFFSET    int   = C.MAX_FILE_OFFSET
	TARGET_PATH_LEN    int   = C.TARGET_PATH_LEN
)

type BpfConf struct {
	bpfModule         *bcc.Module
	perfMapSize       int
	perfMap           *bcc.PerfMap
	table_data        *bcc.Table
	table_files       *bcc.Table
	totalCPUs         int
	diskLostEvents    uint32
	socketLostEvents  uint32
	processLostEvents uint32
	otherLostEvents   uint32
}

type key struct {
	key uint32
}

type BpfStatsInfo struct {
	EventType uint32 `json:"-"`
	Event     string `json:"event,omitempty"`
	Calls     uint32 `json:"calls"`
	Returned  uint32 `json:"returned"`
	Errors    uint32 `json:"errors"`
	Lost      uint32 `json:"lost"`
	Discarded uint32 `json:"discarded"`
}

func PrepareBpf(conf *config.TConfiguration) (*BpfConf, error) {
	var header_file string
	var source_file string
	header_file = "bpfprogram.h"
	if conf.DetailedData {
		source_file = "detailed/bpfprogram.c"
	} else {
		source_file = "raw/bpfprogram.c"
	}

	structs_bytes, err := resources.ReadFile("resources/" + header_file)
	if err != nil {
		return nil, fmt.Errorf("failed to read file 'bpfprogram.h': %v", err)
	}
	structs := string(structs_bytes)
	bpfProgram_bytes, err := resources.ReadFile("resources/" + source_file)
	if err != nil {
		return nil, fmt.Errorf("failed to read file 'bpfprogram.c'%v", err)
	}
	bpfProgram := string(bpfProgram_bytes)

	bpfProgram = strings.Replace(bpfProgram, "//HEADER_CONTENT//", structs, -1)

	// Replace filters and load bpfprogram
	var sb strings.Builder
	fmt.Fprintf(&sb, "\tDIO_PATH_EVENT = %d,\n", event.DIO_PATH_EVENT)
	for syscall_id, syscall_info := range event.SuportedEvents {
		fmt.Fprintf(&sb, "\tDIO_%s = %d,\n", strings.ToUpper(syscall_info.Name), syscall_id)
	}
	bpfProgram = strings.Replace(bpfProgram, "//ENUM_TYPE//", sb.String(), -1)
	bpfProgram = strings.Replace(bpfProgram, "//TARGET_PATHS_LEN//", strconv.Itoa(len(conf.TargetPaths)), -1)
	bpfProgram = strings.Replace(bpfProgram, "//TOTAL_EVENTS//", strconv.Itoa(len(event.SuportedEvents)+1), -1)

	if utils.CheckKernelVersion() {
		bpfProgram = strings.Replace(bpfProgram, "//MAX_JUMPS//", "45", -1)
	} else {
		bpfProgram = strings.Replace(bpfProgram, "//MAX_JUMPS//", "25", -1)
	}

	if conf.TargetCommand != "" {
		var sb_comm strings.Builder
		fmt.Fprintf(&sb_comm, "\"%s\"", conf.TargetCommand)
		bpfProgram = strings.Replace(bpfProgram, "//COMM_FILTER//", strconv.Itoa(1), -1)
		bpfProgram = strings.Replace(bpfProgram, "//TARGET_COMM//", sb_comm.String(), -1)
		utils.InfoLogger.Printf("Filtering by command: %v\n", conf.TargetCommand)
	} else {
		bpfProgram = strings.Replace(bpfProgram, "//COMM_FILTER//", strconv.Itoa(0), -1)
		bpfProgram = strings.Replace(bpfProgram, "//TARGET_COMM//", "\"\"", -1)
	}

	// check PID and TID configs
	if len(conf.TargetPids) > 0 && !conf.TraceAllProcesses {
		utils.InfoLogger.Printf("Filtering by PID: %v\n", conf.TargetPids)
		bpfProgram = strings.Replace(bpfProgram, "//PID_FILTER//", "1", -1)
	} else {
		bpfProgram = strings.Replace(bpfProgram, "//PID_FILTER//", "0", -1)
	}

	if len(conf.TargetTids) > 0 {
		utils.InfoLogger.Printf("Filtering by TID: %v\n", conf.TargetTids)
		bpfProgram = strings.Replace(bpfProgram, "//TID_FILTER//", "1", -1)
	} else {
		bpfProgram = strings.Replace(bpfProgram, "//TID_FILTER//", "0", -1)
	}

	if conf.OldChildren {
		bpfProgram = strings.Replace(bpfProgram, "//CHILDS_FILTER//", "1", -1)
	} else {
		bpfProgram = strings.Replace(bpfProgram, "//CHILDS_FILTER//", "0", -1)
	}

	if conf.DiscardErrors {
		bpfProgram = strings.Replace(bpfProgram, "//DISCARD_ERRORS//", "1", -1)
	} else {
		bpfProgram = strings.Replace(bpfProgram, "//DISCARD_ERRORS//", "0", -1)
	}

	if conf.CaptureProcessEvents {
		bpfProgram = strings.Replace(bpfProgram, "//CAPTURE_PROC_EVENTS//", "#define CAPTURE_PROC_EVENTS 1", -1)
	}

	if conf.SaveTimestamps {
		bpfProgram = strings.Replace(bpfProgram, "//PROFILE_ON//", "#define PROFILE_ON 1", -1)
	}

	if conf.DetailedData {

		if conf.DiscardDirectories {
			bpfProgram = strings.Replace(bpfProgram, "//DISCARD_DIRECTORIES//", "1", -1)
		} else {
			bpfProgram = strings.Replace(bpfProgram, "//DISCARD_DIRECTORIES//", "0", -1)
		}

		if len(conf.TargetPaths) > 0 {
			utils.InfoLogger.Printf("TargetPaths: %v\n", conf.TargetPaths)
			bpfProgram = strings.Replace(bpfProgram, "//FILTER_FILES//", "#define FILTER_FILES 1", -1)
		}

		if conf.MapsStrategy == "one2many" {
			bpfProgram = strings.Replace(bpfProgram, "//ONE2MANY//", "#define ONE2MANY 1", -1)
		}

		if conf.DetailedWithContent == "kernel_hash" {
			bpfProgram = strings.Replace(bpfProgram, "//COMPUTE_HASH//", "#define COMPUTE_HASH 2", -1)
			utils.InfoLogger.Println("Using xxhash32 hash function in kernel space")
		} else if conf.DetailedWithContent == "userspace_hash" {
			bpfProgram = strings.Replace(bpfProgram, "//COMPUTE_HASH//", "#define COMPUTE_HASH 1", -1)
			utils.InfoLogger.Println("Using xxhash32 hash function in user space")
		} else if conf.DetailedWithContent == "plain" {
			bpfProgram = strings.Replace(bpfProgram, "//COMPUTE_HASH//", "#define COMPUTE_HASH 1", -1)
		} else {
			bpfProgram = strings.Replace(bpfProgram, "//COMPUTE_HASH//", "#define COMPUTE_HASH 0", -1)
		}

		if conf.DetailedWithArgPaths {
			bpfProgram = strings.Replace(bpfProgram, "//CAPTURE_ARG_PATHS//", "#define CAPTURE_ARG_PATHS 1", -1)
		}

		if conf.DetailedWithSockAddr {
			bpfProgram = strings.Replace(bpfProgram, "//TRACE_SOCKADDR//", "#define TRACE_SOCKADDR 1", -1)
		}

		if conf.DetailedWithSockData {
			bpfProgram = strings.Replace(bpfProgram, "//TRACE_SOCKDATA//", "#define TRACE_SOCKDATA 1", -1)
		}

	}

	utils.ProfilingStartMeasurement("init_bpf_module")
	bpfConf := &BpfConf{perfMapSize: conf.PerfMapSize, totalCPUs: runtime.NumCPU()}
	bpfConf.bpfModule = bcc.NewModule(bpfProgram, []string{})
	utils.ProfilingStopMeasurement("init_bpf_module")

	return bpfConf, nil
}

func (bpfConf *BpfConf) AttachProbes(events2trace []string, rawTracing bool) error {

	if len(events2trace) == 1 && events2trace[0] == "all" {
		// Trace all events
		for _, eventInfo := range event.SuportedEvents {
			if eventInfo.ID == event.DIO_DESTROY_INODE && rawTracing {
				continue
			}
			err := bpfConf.attachEventProbes(eventInfo.Probes)
			if err != nil {
				return err
			}
		}
	} else if len(events2trace) == 1 && events2trace[0] == "storage" {
		// Trace only storage events
		for _, eventInfo := range event.SuportedEvents {
			if eventInfo.ID == event.DIO_DESTROY_INODE && rawTracing {
				continue
			}
			if eventInfo.Enable || eventInfo.EventType == "storage" {
				err := bpfConf.attachEventProbes(eventInfo.Probes)
				if err != nil {
					return err
				}
			}
		}
	} else if len(events2trace) == 1 && events2trace[0] == "network" {
		// Trace only network events
		for _, eventInfo := range event.SuportedEvents {
			if eventInfo.ID == event.DIO_DESTROY_INODE && rawTracing {
				continue
			}
			if eventInfo.Enable || eventInfo.EventType == "network" {
				err := bpfConf.attachEventProbes(eventInfo.Probes)
				if err != nil {
					return err
				}
			}
		}
	} else {
		// Trace enabled events
		for _, eventInfo := range event.SuportedEvents {
			if eventInfo.ID == event.DIO_DESTROY_INODE && rawTracing {
				continue
			}
			if eventInfo.Enable {
				err := bpfConf.attachEventProbes(eventInfo.Probes)
				if err != nil {
					return err
				}
			}
		}
		// And the events provided as input
		for _, name := range events2trace {
			eventInfo, ok := event.GetEventInfo(name)
			if ok {
				if !eventInfo.Enable {
					err := bpfConf.attachEventProbes(eventInfo.Probes)
					if err != nil {
						return err
					}
				}
			}
		}
	}
	return nil
}

func (bpfConf *BpfConf) SetupTargetPaths(target_paths []string) error {
	var index uint32 = 0
	target_paths_array := bcc.NewTable(bpfConf.bpfModule.TableId("target_paths_array"), bpfConf.bpfModule)
	for _, filename := range target_paths {
		filename_len := len(filename)
		if filename_len > 60 {
			utils.WarningLogger.Println("target_paths filename length > 60 characters!!")
		}
		key := make([]byte, 4)
		bcc.GetHostByteOrder().PutUint32(key, index)
		cname := [C.int(TARGET_PATH_LEN)]C.char{}
		for i := 0; i < filename_len && i < TARGET_PATH_LEN; i++ {
			cname[i] = C.char(filename[i])
		}
		path := C.TargetPathEntry{size: (C.int)(filename_len), name: cname}
		var leaf_bytes bytes.Buffer
		binary.Write(&leaf_bytes, bcc.GetHostByteOrder(), path)
		if err := target_paths_array.Set(key, leaf_bytes.Bytes()); err != nil {
			return fmt.Errorf("failed to add path '%v' to target_paths table: %v", filename, err)
		}
		index += 1
	}
	return nil
}

func (bpfConf *BpfConf) SetupPidList(pids []int) error {
	var zero = ' '
	pids_array := bcc.NewTable(bpfConf.bpfModule.TableId("trace_pids"), bpfConf.bpfModule)
	for _, pid := range pids {
		key := make([]byte, 4)
		bcc.GetHostByteOrder().PutUint32(key, uint32(pid))
		var leaf_bytes bytes.Buffer
		binary.Write(&leaf_bytes, bcc.GetHostByteOrder(), zero)
		if err := pids_array.Set(key, leaf_bytes.Bytes()); err != nil {
			return fmt.Errorf("failed to add pid %v to trace_pids table: %v", pid, err)
		}
	}
	return nil
}

func (bpfConf *BpfConf) SetupTidList(tids []int) error {
	var zero = ' '
	tids_array := bcc.NewTable(bpfConf.bpfModule.TableId("trace_tids"), bpfConf.bpfModule)
	for _, tid := range tids {
		key := make([]byte, 4)
		bcc.GetHostByteOrder().PutUint32(key, uint32(tid))
		var leaf_bytes bytes.Buffer
		binary.Write(&leaf_bytes, bcc.GetHostByteOrder(), zero)
		if err := tids_array.Set(key, leaf_bytes.Bytes()); err != nil {
			return fmt.Errorf("failed to add tid %v to trace_tids table: %v", tid, err)
		}
	}
	return nil
}

func (bpfConf *BpfConf) OpenBpfTables(eventChan chan []byte) error {
	var err error
	table := bcc.NewTable(bpfConf.bpfModule.TableId("events"), bpfConf.bpfModule)
	bpfConf.table_data = bcc.NewTable(bpfConf.bpfModule.TableId("percpu_array_content"), bpfConf.bpfModule)
	bpfConf.table_files = bcc.NewTable(bpfConf.bpfModule.TableId("percpu_array_files"), bpfConf.bpfModule)

	bpfConf.perfMap, err = bcc.InitPerfMapWithPageCnt(table, eventChan, nil, bpfConf.perfMapSize)
	if err != nil {
		return fmt.Errorf("failed to init perf map: %s", err)
	}
	return nil
}

func (bpfConf *BpfConf) StartPooling() {
	bpfConf.perfMap.Start()
}

func (bpfConf *BpfConf) StopPooling() {
	bpfConf.perfMap.Stop()
}

func (bpfConf *BpfConf) Close() {
	bpfConf.bpfModule.Close()
}

func (bpfConf *BpfConf) attachTracepoint(tracepointName string, functionName string) error {
	tracepoint, err := bpfConf.bpfModule.LoadTracepoint(functionName)
	if err != nil {
		return fmt.Errorf("failed to load tracepoint '%s' for '%s': %s\n", functionName, tracepointName, err)
	}
	if err := bpfConf.bpfModule.AttachTracepoint(tracepointName, tracepoint); err != nil {
		return fmt.Errorf("failed to attach function %s to tracepoint %s: %s\n", functionName, tracepointName, err)
	}
	return nil
}

func (bpfConf *BpfConf) attachKprobe(kprobeName string, functionName string) error {
	kprobe, err := bpfConf.bpfModule.LoadKprobe(functionName)
	if err != nil {
		return fmt.Errorf("failed to load kprobe '%s': %s\n", functionName, err)
	}
	err = bpfConf.bpfModule.AttachKprobe(kprobeName, kprobe, -1)
	if err != nil {
		return fmt.Errorf("failed to attach function %s to kprobe %s : %s\n", functionName, kprobeName, err)
	}
	return nil
}

func (bpfConf *BpfConf) attachKretprobe(KretprobeName string, functionName string) error {
	Kretprobe, err := bpfConf.bpfModule.LoadKprobe(functionName)
	if err != nil {
		return fmt.Errorf("failed to load Kretprobe '%s': %s\n", functionName, err)
	}
	err = bpfConf.bpfModule.AttachKretprobe(KretprobeName, Kretprobe, -1)
	if err != nil {
		return fmt.Errorf("failed to attach function %s to Kretprobe %s : %s\n", functionName, KretprobeName, err)
	}
	return nil
}

func (bpfConf *BpfConf) attachEventProbes(probes []event.Probe) error {
	for _, probe := range probes {

		if probe.ProbeType == "tracepoint" {
			err := bpfConf.attachTracepoint(probe.ProbeName, probe.FunctionName)
			if err != nil {
				return err
			}
		} else if probe.ProbeType == "kprobe" {
			err := bpfConf.attachKprobe(probe.ProbeName, probe.FunctionName)
			if err != nil {
				return err
			}
		} else if probe.ProbeType == "kretprobe" {
			err := bpfConf.attachKretprobe(probe.ProbeName, probe.FunctionName)
			if err != nil {
				return err
			}
		} else {
			utils.WarningLogger.Printf("Unknown probe type: %v. Skipping...\n", probe.ProbeType)
			continue
		}
	}
	return nil
}

func CheckEventContext(buf *bytes.Buffer) error {
	if buf.Len() < (C.sizeof_EventBase - 4) {
		return fmt.Errorf("expected buf.Len() >= %d, but got %d", C.sizeof_EventBase-4, buf.Len())
	}
	return nil
}

func (bpfConf *BpfConf) GetDataContent(buf *bytes.Buffer, size uint64, cpu uint16) (string, error) {
	if size == 0 {
		return "", nil
	}
	// get table file descriptor
	index := bcc.GetHostByteOrder().Uint32(buf.Next(4))
	ref := bcc.GetHostByteOrder().Uint32(buf.Next(4))

	fd := C.int(bpfConf.table_data.Config()["fd"].(int))

	// key corresponds to event index
	k := make([]byte, 4)
	bcc.GetHostByteOrder().PutUint32(k, index)
	keyP := unsafe.Pointer(&k[0])

	// prepare leaf
	leafSize := bpfConf.table_data.Config()["leaf_size"].(uint64)
	leaf := make([]byte, leafSize*uint64(bpfConf.totalCPUs))
	leafP := unsafe.Pointer(&leaf[0])

	// lookup elem
	r, err := C.bpf_lookup_elem(fd, keyP, leafP)
	if r != 0 {
		return "", fmt.Errorf("incomplete event: %v", err)
	}

	var contentBuff *bytes.Buffer
	if int(cpu) < 0 || int(cpu) > bpfConf.totalCPUs {
		return "", fmt.Errorf("wrong cpu value. Got %v, expected < %v\n", cpu, bpfConf.totalCPUs)
	}
	start := uint64(cpu) * leafSize
	end := (uint64(cpu) + 1) * leafSize

	if leafSize < C.sizeof_Message_Content {
		return "", fmt.Errorf("expected buf.Len() >= %d, but got %d", C.sizeof_Message_Content, leafSize)
	}

	buf_end := start + 4 + size
	if buf_end > end {
		return "", fmt.Errorf("Content: buffer end (%v) > leafSize end (%v)", buf_end, end)
	}

	contentBuff = bytes.NewBuffer(leaf[start:buf_end])

	// parse event data (content)

	m_ref := bcc.GetHostByteOrder().Uint32(contentBuff.Next(4))
	// check is ref is correct. Otherwise mark event as incomplete
	if ref != m_ref {
		return "", fmt.Errorf("incomplete event: Failed to get data content")
	}

	msgBytes := contentBuff.Next(int(size))
	msgCstr := (*C.char)(unsafe.Pointer(&msgBytes[0]))
	msg := C.GoStringN(msgCstr, C.int(size))

	return msg, nil
}

func (bpfConf *BpfConf) GetCommStr(buf *bytes.Buffer) string {
	commBytes := buf.Next(TASK_COMM_LEN)
	commCstr := (*C.char)(unsafe.Pointer(&commBytes[0]))
	comm := C.GoString(commCstr)
	return comm
}

func (bpfConf *BpfConf) GetXattrName(buf *bytes.Buffer) string {
	nameBytes := buf.Next(MAX_ATTR_NAME_SIZE)
	nameCstr := (*C.char)(unsafe.Pointer(&nameBytes[0]))
	name := C.GoString(nameCstr)
	return name
}

func (bpfConf *BpfConf) GetFileInfo(index uint32, cpu uint16, ref uint32, ino uint32) (*path.EventPath, error) {
	// get table file descriptor
	fd := C.int(bpfConf.table_files.Config()["fd"].(int))

	// key corresponds to event index
	k := make([]byte, 4)
	bcc.GetHostByteOrder().PutUint32(k, index)
	keyP := unsafe.Pointer(&k[0])

	// prepare leaf
	leafSize := bpfConf.table_files.Config()["leaf_size"].(uint64)
	leaf := make([]byte, leafSize*uint64(bpfConf.totalCPUs))
	leafP := unsafe.Pointer(&leaf[0])

	// lookup elem
	r, err := C.bpf_lookup_elem(fd, keyP, leafP)
	if r != 0 {
		return nil, fmt.Errorf("bpf_lookup_elem failed: %v", err)
	}
	// get the correct cpu item
	var fileInfo *bytes.Buffer
	if int(cpu) < 0 || int(cpu) > bpfConf.totalCPUs {
		return nil, fmt.Errorf("wrong cpu value. Got %v, expected < %v\n", cpu, bpfConf.totalCPUs)
	}

	file := &path.EventPath{}
	file.DocType = "EventPath"

	start := uint64(cpu) * leafSize
	end := (uint64(cpu) + 1) * leafSize

	if leafSize < C.sizeof_FileInfo {
		return nil, fmt.Errorf("expected buf.Len() >= %d, but got %d", C.sizeof_FileInfo, leafSize)
	}

	meta_end := start + 16
	fileInfo = bytes.NewBuffer(leaf[start:meta_end])

	n_ref := bcc.GetHostByteOrder().Uint32(fileInfo.Next(4))
	if ref != n_ref {
		return nil, fmt.Errorf("incomplete event: Failed to get file data")
	}

	file_type := bcc.GetHostByteOrder().Uint16(fileInfo.Next(2))
	file.FileType = estorage.GetFileType(file_type)
	fileInfo.Next(2) // padding

	offset := uint64(bcc.GetHostByteOrder().Uint32(fileInfo.Next(4)))
	size := uint64(bcc.GetHostByteOrder().Uint32(fileInfo.Next(4)))
	if size > 0 {
		buf_start := meta_end + offset
		buf_end := buf_start + size
		if buf_end > end {
			return nil, fmt.Errorf("filename end > leafSize end")
		}
		fileInfo = bytes.NewBuffer(leaf[buf_start:buf_end])
		filenameBytes := fileInfo.Next(int(size))
		filenameCstr := (*C.char)(unsafe.Pointer(&filenameBytes[0]))
		filename := C.GoStringN(filenameCstr, C.int(size)) //TODO: GoString sem n?

		if file.FileType == "Socket" {
			file.Filename = fmt.Sprintf("%s:[%v]", filename, ino)
		} else {
			file.Filename = filename
		}

	} else if file.FileType == "Pipe" {
		file.Filename = fmt.Sprintf("pipe:[%v]", ino)
	}

	return file, nil
}

func (bpfConf *BpfConf) GetDoublePathArgs(index uint32, cpu uint16, ref uint32, oldname_len uint32, newname_len uint32) (string, string, error) {
	// get table file descriptor
	fd := C.int(bpfConf.table_files.Config()["fd"].(int))

	// key corresponds to event index
	k := make([]byte, 4)
	bcc.GetHostByteOrder().PutUint32(k, index)
	keyP := unsafe.Pointer(&k[0])

	// prepare leaf
	leafSize := bpfConf.table_files.Config()["leaf_size"].(uint64)
	leaf := make([]byte, leafSize*uint64(bpfConf.totalCPUs))
	leafP := unsafe.Pointer(&leaf[0])

	// lookup elem
	r, err := C.bpf_lookup_elem(fd, keyP, leafP)
	if r != 0 {
		return "", "", fmt.Errorf("bpf_lookup_elem failed: %v", err)
	}
	// get the correct cpu item
	var data *bytes.Buffer
	if int(cpu) < 0 || int(cpu) > bpfConf.totalCPUs {
		return "", "", fmt.Errorf("wrong cpu value. Got %v, expected < %v\n", cpu, bpfConf.totalCPUs)
	}

	start := uint64(cpu) * leafSize
	end := (uint64(cpu) + 1) * leafSize

	if leafSize < C.sizeof_FileInfo {
		return "", "", fmt.Errorf("expected buf.Len() >= %d, but got %d", C.sizeof_FileInfo, leafSize)
	}

	end_buf := start + 16 + uint64(oldname_len) + uint64(newname_len)
	if end_buf > end {
		return "", "", fmt.Errorf("oldname: filename end (%v) > leafSize end (%v)", end_buf, end)
	}

	data = bytes.NewBuffer(leaf[start:end_buf])
	n_ref := bcc.GetHostByteOrder().Uint32(data.Next(4))
	if ref != n_ref {
		return "", "", fmt.Errorf("incomplete event: Failed to get file data")
	}
	data.Next(12) // padding

	dataBytes := data.Next(int(oldname_len))
	dataCstr := (*C.char)(unsafe.Pointer(&dataBytes[0]))
	oldname := C.GoStringN(dataCstr, C.int(oldname_len))

	if newname_len > 0 {
		dataBytes = data.Next(int(newname_len))
		dataCstr = (*C.char)(unsafe.Pointer(&dataBytes[0]))
		newname := C.GoString(dataCstr)
		return oldname, newname, nil
	}

	return oldname, "", nil
}

func (bpfConf *BpfConf) GetPathArgs(index uint32, cpu uint16, ref uint32) (string, error) {
	// get table file descriptor
	fd := C.int(bpfConf.table_files.Config()["fd"].(int))

	// key corresponds to event index
	k := make([]byte, 4)
	bcc.GetHostByteOrder().PutUint32(k, index)
	keyP := unsafe.Pointer(&k[0])

	// prepare leaf
	leafSize := bpfConf.table_files.Config()["leaf_size"].(uint64)
	leaf := make([]byte, leafSize*uint64(bpfConf.totalCPUs))
	leafP := unsafe.Pointer(&leaf[0])

	// lookup elem
	r, err := C.bpf_lookup_elem(fd, keyP, leafP)
	if r != 0 {
		return "", fmt.Errorf("bpf_lookup_elem failed: %v", err)
	}
	// get the correct cpu item
	var data *bytes.Buffer
	if int(cpu) < 0 || int(cpu) > bpfConf.totalCPUs {
		return "", fmt.Errorf("wrong cpu value. Got %v, expected < %v\n", cpu, bpfConf.totalCPUs)
	}

	start := uint64(cpu) * leafSize
	end := (uint64(cpu) + 1) * leafSize

	if leafSize < C.sizeof_FileInfo {
		return "", fmt.Errorf("expected buf.Len() >= %d, but got %d", C.sizeof_FileInfo, leafSize)
	}

	meta_end := start + 16
	data = bytes.NewBuffer(leaf[start:meta_end])
	n_ref := bcc.GetHostByteOrder().Uint32(data.Next(4))
	if ref != n_ref {
		return "", fmt.Errorf("incomplete event: Failed to get file data")
	}
	data.Next(8)

	size := bcc.GetHostByteOrder().Uint32(data.Next(4))
	if size <= 0 {
		return "", nil
	}
	start_buf := meta_end
	end_buf := start_buf + uint64(size)
	if end_buf > end {
		return "", fmt.Errorf("arg path: path end (%v) > leafSize end (%v)", end_buf, end)
	}
	data = bytes.NewBuffer(leaf[start_buf:end_buf])
	dataBytes := data.Next(int(size))
	dataCstr := (*C.char)(unsafe.Pointer(&dataBytes[0]))
	path := C.GoString(dataCstr)

	return path, nil
}

func (bpfConf *BpfConf) GetBpfStats() (map[uint32]BpfStatsInfo, error) {
	entry_table := bcc.NewTable(bpfConf.bpfModule.TableId("entries_stats"), bpfConf.bpfModule)
	exit_table := bcc.NewTable(bpfConf.bpfModule.TableId("exits_stats"), bpfConf.bpfModule)
	error_table := bcc.NewTable(bpfConf.bpfModule.TableId("errors_stats"), bpfConf.bpfModule)
	lost_table := bcc.NewTable(bpfConf.bpfModule.TableId("losts_stats"), bpfConf.bpfModule)
	discarded_table := bcc.NewTable(bpfConf.bpfModule.TableId("discarded_stats"), bpfConf.bpfModule)

	stats := make(map[uint32]BpfStatsInfo)

	// get entry stats
	iter := entry_table.Iter()
	for iter.Next() {
		key, leaf := iter.Key(), iter.Leaf()

		var k uint32
		var v uint32
		if err := binary.Read(bytes.NewBuffer(key), bcc.GetHostByteOrder(), &k); err != nil {
			return nil, fmt.Errorf("failed to get stats key: cannot decode key: %v", err)
		}
		if err := binary.Read(bytes.NewBuffer(leaf), bcc.GetHostByteOrder(), &v); err != nil {
			return nil, fmt.Errorf("failed to get stats value: cannot decode value: %v", err)
		}

		newStatsStruct := BpfStatsInfo{}
		newStatsStruct.EventType = k
		newStatsStruct.Event = event.GetEventName(k)
		newStatsStruct.Calls = v
		stats[k] = newStatsStruct
	}
	if iter.Err() != nil {
		return nil, fmt.Errorf("failed to get calls stats: iteration finished with unexpected error: %v", iter.Err())
	}

	// get exit stats
	iter = exit_table.Iter()
	for iter.Next() {
		key, leaf := iter.Key(), iter.Leaf()

		var k uint32
		var v uint32
		if err := binary.Read(bytes.NewBuffer(key), bcc.GetHostByteOrder(), &k); err != nil {
			return nil, fmt.Errorf("failed to get stats key: cannot decode key: %v", err)
		}
		if err := binary.Read(bytes.NewBuffer(leaf), bcc.GetHostByteOrder(), &v); err != nil {
			return nil, fmt.Errorf("failed to get stats value: cannot decode value: %v", err)
		}

		curStatsStruct := stats[k]
		curStatsStruct.Returned = v
		stats[k] = curStatsStruct
	}
	if iter.Err() != nil {
		return nil, fmt.Errorf("failed to get returns stats: finished with unexpected error: %v", iter.Err())
	}

	// get error stats
	iter = error_table.Iter()
	for iter.Next() {
		key, leaf := iter.Key(), iter.Leaf()

		var k uint32
		var v uint32
		if err := binary.Read(bytes.NewBuffer(key), bcc.GetHostByteOrder(), &k); err != nil {
			return nil, fmt.Errorf("failed to get stats key: cannot decode key: %v", err)
		}
		if err := binary.Read(bytes.NewBuffer(leaf), bcc.GetHostByteOrder(), &v); err != nil {
			return nil, fmt.Errorf("failed to get stats value: cannot decode value: %v", err)
		}

		curStatsStruct := stats[k]
		curStatsStruct.Errors = v
		stats[k] = curStatsStruct
	}
	if iter.Err() != nil {
		return nil, fmt.Errorf("failed to get errors stats: finished with unexpected error: %v", iter.Err())
	}

	// get lost stats
	iter = lost_table.Iter()
	for iter.Next() {
		key, leaf := iter.Key(), iter.Leaf()

		var k uint32
		var v uint32
		if err := binary.Read(bytes.NewBuffer(key), bcc.GetHostByteOrder(), &k); err != nil {
			return nil, fmt.Errorf("failed to get stats key: cannot decode key: %v", err)
		}
		if err := binary.Read(bytes.NewBuffer(leaf), bcc.GetHostByteOrder(), &v); err != nil {
			return nil, fmt.Errorf("failed to get stats value: cannot decode value: %v", err)
		}

		curStatsStruct := stats[k]
		curStatsStruct.Lost = v
		stats[k] = curStatsStruct
	}
	if iter.Err() != nil {
		return nil, fmt.Errorf("failed to get lost stats: finished with unexpected error: %v", iter.Err())
	}

	// get discarded stats
	iter = discarded_table.Iter()
	for iter.Next() {
		key, leaf := iter.Key(), iter.Leaf()

		var k uint32
		var v uint32
		if err := binary.Read(bytes.NewBuffer(key), bcc.GetHostByteOrder(), &k); err != nil {
			return nil, fmt.Errorf("failed to get stats key: cannot decode key: %v", err)
		}
		if err := binary.Read(bytes.NewBuffer(leaf), bcc.GetHostByteOrder(), &v); err != nil {
			return nil, fmt.Errorf("failed to get stats value: cannot decode value: %v", err)
		}

		curStatsStruct := stats[k]
		curStatsStruct.Discarded = v
		stats[k] = curStatsStruct
	}
	if iter.Err() != nil {
		return nil, fmt.Errorf("failed to get discarded stats: finished with unexpected error: %v", iter.Err())
	}

	return stats, nil
}

func (bpfConf *BpfConf) GetTimesLogs() (map[uint64]uint64, map[uint64]uint64, map[uint64]uint64, error) {
	call_table := bcc.NewTable(bpfConf.bpfModule.TableId("times_logs_call"), bpfConf.bpfModule)
	subm_table := bcc.NewTable(bpfConf.bpfModule.TableId("times_logs_subm"), bpfConf.bpfModule)
	lost_table := bcc.NewTable(bpfConf.bpfModule.TableId("times_logs_lost"), bpfConf.bpfModule)

	calls := make(map[uint64]uint64)
	submitted := make(map[uint64]uint64)
	lost := make(map[uint64]uint64)

	// get submitted times logs
	iter := call_table.Iter()
	for iter.Next() {
		key, leaf := iter.Key(), iter.Leaf()

		var k uint64
		var v uint64
		if err := binary.Read(bytes.NewBuffer(key), bcc.GetHostByteOrder(), &k); err != nil {
			return calls, submitted, lost, fmt.Errorf("failed to get logs key: cannot decode key: %v", err)
		}
		if err := binary.Read(bytes.NewBuffer(leaf), bcc.GetHostByteOrder(), &v); err != nil {
			return calls, submitted, lost, fmt.Errorf("failed to get logs value: cannot decode value: %v", err)
		}
		calls[k] = v
	}
	if iter.Err() != nil {
		return calls, submitted, lost, fmt.Errorf("failed to get calls stats: iteration finished with unexpected error: %v", iter.Err())
	}

	// get submitted times logs
	iter = subm_table.Iter()
	for iter.Next() {
		key, leaf := iter.Key(), iter.Leaf()

		var k uint64
		var v uint64
		if err := binary.Read(bytes.NewBuffer(key), bcc.GetHostByteOrder(), &k); err != nil {
			return calls, submitted, lost, fmt.Errorf("failed to get logs key: cannot decode key: %v", err)
		}
		if err := binary.Read(bytes.NewBuffer(leaf), bcc.GetHostByteOrder(), &v); err != nil {
			return calls, submitted, lost, fmt.Errorf("failed to get logs value: cannot decode value: %v", err)
		}
		submitted[k] = v
	}
	if iter.Err() != nil {
		return calls, submitted, lost, fmt.Errorf("failed to get calls stats: iteration finished with unexpected error: %v", iter.Err())
	}

	// get submitted times logs
	iter = lost_table.Iter()
	for iter.Next() {
		key, leaf := iter.Key(), iter.Leaf()

		var k uint64
		var v uint64
		if err := binary.Read(bytes.NewBuffer(key), bcc.GetHostByteOrder(), &k); err != nil {
			return calls, submitted, lost, fmt.Errorf("failed to get logs key: cannot decode key: %v", err)
		}
		if err := binary.Read(bytes.NewBuffer(leaf), bcc.GetHostByteOrder(), &v); err != nil {
			return calls, submitted, lost, fmt.Errorf("failed to get logs size: cannot decode value: %v", err)
		}
		lost[k] = v
	}
	if iter.Err() != nil {
		return calls, submitted, lost, fmt.Errorf("failed to get calls stats: iteration finished with unexpected error: %v", iter.Err())
	}

	return calls, submitted, lost, nil
}

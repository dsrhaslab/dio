package stats

import (
	"fmt"
	"os"
	"reflect"
	"strings"
	"sync"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/ebpfs"
	event "github.com/dsrhaslab/dio/dio-tracer/pkg/event"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"
)

var mutex sync.RWMutex
var es_flushed uint64

type SavedStats struct {
	Network uint32
	Storage uint32
	Process uint32
	Path    uint32
	Other   uint32
	Total   uint32
}

type StatsByType struct {
	Network TracerStats `json:"network,omitempty"`
	Storage TracerStats `json:"storage,omitempty"`
	Process TracerStats `json:"process,omitempty"`
	Path    TracerStats `json:"path,omitempty"`
	Other   TracerStats `json:"others,omitempty"`
}

type TracerStats struct {
	Calls      uint32 `json:"calls"`
	Returned   uint32 `json:"returned"`
	Discard    uint32 `json:"discarded"`
	Lost       uint32 `json:"lost"`
	Errors     uint32 `json:"errors"`
	Handled    uint32 `json:"handled"`
	Incomplete uint32 `json:"incomplete"`
	Truncated  uint32 `json:"truncated"`
	Saved      uint32 `json:"saved"`
	ES_Flushed uint32 `json:"es_flushed,omitempty"`
}

type Stats struct {
	SessionID        string               `json:"session_name"`
	BpfStats         []ebpfs.BpfStatsInfo `json:"bpf_stats"`
	BpfStatsTotal    ebpfs.BpfStatsInfo   `json:"bpf_stats_total"`
	TracerStats      StatsByType          `json:"tracer_stats"`
	TracerStatsTotal TracerStats          `json:"tracer_stats_total"`
}

func (stats *Stats) UpdateHandledEvents(e_type uint32) {
	if event.IsNetworkEvent(e_type) {
		stats.TracerStats.Network.Handled += 1
	} else if event.IsStorageEvent(e_type) {
		stats.TracerStats.Storage.Handled += 1
	} else if event.IsProcessEvent(e_type) {
		stats.TracerStats.Process.Handled += 1
	} else if event.IsPathEvent(e_type) {
		stats.TracerStats.Path.Handled += 1
	} else {
		stats.TracerStats.Other.Handled += 1
	}
	stats.TracerStatsTotal.Handled += 1
}
func (stats *Stats) UpdateIncompleteEvents(e_type uint32) {
	if event.IsNetworkEvent(e_type) {
		stats.TracerStats.Network.Incomplete += 1
	} else if event.IsStorageEvent(e_type) {
		stats.TracerStats.Storage.Incomplete += 1
	} else if event.IsProcessEvent(e_type) {
		stats.TracerStats.Process.Incomplete += 1
	} else if event.IsPathEvent(e_type) {
		stats.TracerStats.Path.Incomplete += 1
	} else {
		stats.TracerStats.Other.Incomplete += 1
	}
	stats.TracerStatsTotal.Incomplete += 1
}
func (stats *Stats) UpdateTruncateEvents(e_type uint32) {
	if event.IsNetworkEvent(e_type) {
		stats.TracerStats.Network.Truncated += 1
	} else if event.IsStorageEvent(e_type) {
		stats.TracerStats.Storage.Truncated += 1
	} else if event.IsProcessEvent(e_type) {
		stats.TracerStats.Process.Truncated += 1
	} else if event.IsPathEvent(e_type) {
		stats.TracerStats.Path.Truncated += 1
	} else {
		stats.TracerStats.Other.Truncated += 1
	}
	stats.TracerStatsTotal.Truncated += 1
}

func (stats *Stats) PrintStats(bpfConf *ebpfs.BpfConf, statsPath string) error {

	bpfStats, err := bpfConf.GetBpfStats()
	if err != nil {
		return err
	}
	if statsPath != "" {
		stats_file, err := os.OpenFile(statsPath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0644)
		if err != nil {
			return fmt.Errorf("Failed to open output file: %v", err)
		}
		stats.fillBpfStats(bpfStats)
		stats.TracerStatsTotal.ES_Flushed = uint32(es_flushed)
		m, err := utils.JSONMarshalIndent(stats)
		if err != nil {
			return fmt.Errorf("error marshalling json data: %v", err)
		}
		stats_file.Write(m)
		stats_file.Close()
	} else {
		utils.InfoLogger.Printf(strings.Repeat("-", 80))
		printBpfStats(bpfStats)
		utils.InfoLogger.Printf(strings.Repeat("-", 80))
		printTracerStats(stats)
	}
	return nil
}

func printBpfStats(stats map[uint32]ebpfs.BpfStatsInfo) {
	var totalCallEvents uint32 = 0
	var totalReturnedEvents uint32 = 0
	var totalErrorsEvents uint32 = 0
	var totalLostEvents uint32 = 0
	var totalDiscardedEvents uint32 = 0

	utils.InfoLogger.Printf("BPF Stats:\n")
	utils.InfoLogger.Printf("\t%-15s\t%8s\t%8s\t%8s\t%8s\t%8s\n", "Event", "Calls", "Returns", "Errors", "Lost", "Discarded")

	for k, v := range stats {
		if v.Calls > 0 {
			eventName := event.GetEventName(k)
			utils.InfoLogger.Printf("\t%-10s\t%8d\t%8d\t%8d\t%8d\t%8d\n", eventName, v.Calls, v.Returned, v.Errors, v.Lost, v.Discarded)
			totalCallEvents += v.Calls
			totalReturnedEvents += v.Returned
			totalErrorsEvents += v.Errors
			totalLostEvents += v.Lost
			totalDiscardedEvents += v.Discarded
		}
	}
	utils.InfoLogger.Printf("\t%-10s\t%8d\t%8d\t%8d\t%8d\t%8d\n", "TOTAL", totalCallEvents, totalReturnedEvents, totalErrorsEvents, totalLostEvents, totalDiscardedEvents)
}
func printTracerStats(stats *Stats) {

	utils.InfoLogger.Printf("TRACER STATS:")
	utils.InfoLogger.Printf("\t%-10s\t%10s\t%10s\t%10s\t%10s\n", "Event", "Handled", "Incomplete", "Truncated", "Saved")

	val := reflect.ValueOf(stats.TracerStats)
	typeOfS := val.Type()
	for i := 0; i < val.NumField(); i++ {
		v := val.Field(i).Interface().(TracerStats)
		if v.Handled > 0 {
			utils.InfoLogger.Printf("\t%-10s\t%10d\t%10d\t%10d\t%10d\n", typeOfS.Field(i).Name, v.Handled, v.Incomplete, v.Truncated, v.Saved)
		}
	}
	utils.InfoLogger.Printf("\t%-10s\t%10d\t%10d\t%10d\t%10d\n", "TOTAL", stats.TracerStatsTotal.Handled, stats.TracerStatsTotal.Incomplete, stats.TracerStatsTotal.Truncated, stats.TracerStatsTotal.Saved)
}
func (statsAll *Stats) fillBpfStats(stats map[uint32]ebpfs.BpfStatsInfo) {
	statsAll.BpfStats = make([]ebpfs.BpfStatsInfo, 0)
	statsAll.BpfStatsTotal = ebpfs.BpfStatsInfo{}
	for _, v := range stats {
		if v.Calls > 0 {
			statsAll.BpfStatsTotal.Calls += v.Calls
			statsAll.BpfStatsTotal.Returned += v.Returned
			statsAll.BpfStatsTotal.Errors += v.Errors
			statsAll.BpfStatsTotal.Lost += v.Lost
			statsAll.BpfStatsTotal.Discarded += v.Discarded
			statsAll.updateTracerStatsFromBPF(v)
			statsAll.BpfStats = append(statsAll.BpfStats, v)
		}
	}
}

func (statsAll *Stats) updateTracerStatsFromBPF(stats ebpfs.BpfStatsInfo) {
	if event.IsNetworkEvent(stats.EventType) {
		statsAll.TracerStats.Network.Calls += stats.Calls
		statsAll.TracerStats.Network.Returned += stats.Returned
		statsAll.TracerStats.Network.Errors += stats.Errors
		statsAll.TracerStats.Network.Lost += stats.Lost
		statsAll.TracerStats.Network.Discard += stats.Discarded
	} else if event.IsStorageEvent(stats.EventType) {
		statsAll.TracerStats.Storage.Calls += stats.Calls
		statsAll.TracerStats.Storage.Returned += stats.Returned
		statsAll.TracerStats.Storage.Errors += stats.Errors
		statsAll.TracerStats.Storage.Lost += stats.Lost
		statsAll.TracerStats.Storage.Discard += stats.Discarded
	} else if event.IsProcessEvent(stats.EventType) {
		statsAll.TracerStats.Process.Calls += stats.Calls
		statsAll.TracerStats.Process.Returned += stats.Returned
		statsAll.TracerStats.Process.Errors += stats.Errors
		statsAll.TracerStats.Process.Lost += stats.Lost
		statsAll.TracerStats.Process.Discard += stats.Discarded
	} else if event.IsPathEvent(stats.EventType) {
		statsAll.TracerStats.Path.Calls += stats.Calls
		statsAll.TracerStats.Path.Returned += stats.Returned
		statsAll.TracerStats.Path.Errors += stats.Errors
		statsAll.TracerStats.Path.Lost += stats.Lost
		statsAll.TracerStats.Path.Discard += stats.Discarded
	} else {
		statsAll.TracerStats.Other.Calls += stats.Calls
		statsAll.TracerStats.Other.Returned += stats.Returned
		statsAll.TracerStats.Other.Errors += stats.Errors
		statsAll.TracerStats.Other.Lost += stats.Lost
		statsAll.TracerStats.Other.Discard += stats.Discarded
	}
	statsAll.TracerStatsTotal.Calls += stats.Calls
	statsAll.TracerStatsTotal.Returned += stats.Returned
	statsAll.TracerStatsTotal.Errors += stats.Errors
	statsAll.TracerStatsTotal.Lost += stats.Lost
	statsAll.TracerStatsTotal.Discard += stats.Discarded
}

func (stats *SavedStats) UpdateSaved(e_type uint32) {
	if event.IsNetworkEvent(e_type) {
		stats.Network += 1
	} else if event.IsStorageEvent(e_type) {
		stats.Storage += 1
	} else if event.IsProcessEvent(e_type) {
		stats.Process += 1
	} else if event.IsPathEvent(e_type) {
		stats.Path += 1
	} else {
		stats.Other += 1
	}
	stats.Total += 1
}

func (statsAll *Stats) SaveStats(stats SavedStats) {
	mutex.Lock()
	statsAll.TracerStats.Network.Saved += stats.Network
	statsAll.TracerStats.Process.Saved += stats.Process
	statsAll.TracerStats.Storage.Saved += stats.Storage
	statsAll.TracerStats.Path.Saved += stats.Path
	statsAll.TracerStats.Other.Saved += stats.Other
	statsAll.TracerStatsTotal.Saved += stats.Total
	mutex.Unlock()
}

func UpdateESFlushed(flushed uint64) {
	es_flushed += flushed
}

package utils

import (
	"os/exec"
	"strconv"
	"strings"
	"time"
)

var bootTime *int64

func ComputeBootTime() {
	bootTime = getBootTime()
}

func getBootTime() *int64 {
	//Get system boot time
	out, err := exec.Command("stat", "-c", "%Z", "/proc/").Output()
	if err != nil {
		ErrorLogger.Fatalf("Failed to execute command 'uptime -s': %s", err)
	}

	out2 := strings.TrimSuffix(string(out), "\n")
	n, _ := strconv.ParseInt(out2, 10, 64)
	return &n
}

func Epoch2dateTime(ts uint64) time.Time {
	if bootTime == nil {
		bootTime = getBootTime()
	}

	return time.Unix(*bootTime, int64(ts))
}

func Nanosecond2Time(ts uint64) time.Time {
	return time.Unix(0, int64(ts))
}

func Epoch2dateTimeSeconds(ts_seconds uint64) time.Time {
	if bootTime == nil {
		bootTime = getBootTime()
	}

	return time.Unix(*bootTime+int64(ts_seconds), 0)
}

func DateTime2String(time_t time.Time) string {
	return time_t.Format("2006-01-02T15:04:05.000000000Z07")
}

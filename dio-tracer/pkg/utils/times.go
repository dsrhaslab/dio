package utils

import (
	"os/exec"
	"strings"
	"time"
)

var bootTime *time.Time

func ComputeBootTime() {
	bootTime = getBootTime()
}

func getBootTime() *time.Time {
	//Get system boot time

	out, err := exec.Command("uptime", "-s").Output()
	if err != nil {
		ErrorLogger.Fatalf("Failed to execute command 'uptime -s': %s", err)
	}
	lastBootTime := strings.TrimSpace(string(out))
	lastBootTime = strings.TrimPrefix(lastBootTime, "system boot")
	lastBootTime = strings.TrimSpace(lastBootTime)

	out, err = exec.Command("date", "+%Z").Output()
	if err != nil {
		ErrorLogger.Fatalf("Failed to execute command 'date +%%Z': %s", err)
	}
	timezone := strings.TrimSpace(string(out))

	bootTime, err := time.Parse(`2006-01-02 15:04:05MST`, lastBootTime+timezone)
	if err != nil {
		ErrorLogger.Fatalf("Failed to parse bootTime: %s", err)
	}

	return &bootTime
}

func Epoch2dateTime(ts uint64) time.Time {
	if bootTime == nil {
		bootTime = getBootTime()
	}
	return bootTime.Add(time.Duration(ts) * time.Nanosecond)
}

func Nanosecond2Time(ts uint64) time.Time {
	return time.Unix(0, int64(ts))
}

func Epoch2dateTimeSeconds(ts_seconds uint64) time.Time {
	if bootTime == nil {
		bootTime = getBootTime()
	}
	return bootTime.Add(time.Duration(ts_seconds) * time.Second) // TODO: validate this
}

func DateTime2String(time_t time.Time) string {
	return time_t.Format("2006-01-02T15:04:05.000000000Z07")
}

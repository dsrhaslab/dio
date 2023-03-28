package utils

import (
	"fmt"
	"io"
	"log"
	"os"
	"time"
)

type LoggerConfiguration struct {
	Log2Stdout           bool   `yaml:"log2stdout"               env:"DIO_LOG_STDOUT"              env-description:"Print log to stdout"`
	LogFilename          string `yaml:"log_filename"             env:"DIO_LOG_FILENAME"            env-description:"Log filename"`
	Log2File             bool   `yaml:"log2file"                 env:"DIO_LOG_FILE"                env-description:"Save log to file"`
	DebugMode            bool   `yaml:"debug_mode"               env:"DIO_DEBUG_ON"                env-description:"Enable Debug mode"`
	ProfilingEnabled     bool   `yaml:"profiling_on"             env:"DIO_PROFILING_ON"            env-description:"Enable Profiling mode"`
	ProfilingResult      string `yaml:"profiling_result"         env:"DIO_PROFILING_RESULT"        env-description:"Profiling result path"`
	SaveTimestamps       bool   `yaml:"save_timestamps"          env:"DIO_SAVE_TIMESTAMPS"         env-description:"Save timestamps"`
	ProfilingTimesResult string `yaml:"profiling_times_result"   env:"DIO_PROFILING_TIMES_RESULT"  env-description:"Profiling times result path"`
}

type measument struct {
	Name        string        `json:"-"`
	StartTime   time.Time     `json:"-"`
	ElapsedTime time.Duration `json:"elapsed_time_ns"`
}

type ProfilingData struct {
	SessionID  string                   `json:"session_name"`
	Measuments map[string]time.Duration `json:"profiling_data"`
}

var (
	DebugLogger    *log.Logger
	InfoLogger     *log.Logger
	WarningLogger  *log.Logger
	ErrorLogger    *log.Logger
	times          map[string]measument
	logFile        *os.File
	LoggerConf     *LoggerConfiguration
	timestampsFile *os.File
)

func SetupLogger(conf *LoggerConfiguration) error {
	var out io.Writer
	var err error = nil

	if conf.Log2File && conf.Log2Stdout {
		logFile, err = os.OpenFile(conf.LogFilename, os.O_RDWR|os.O_CREATE|os.O_APPEND, 0666)
		if err != nil {
			return fmt.Errorf("failed to open log file %s: %v", conf.LogFilename, err)
		}
		out = io.MultiWriter(os.Stdout, logFile)
		log.SetOutput(out)
	} else if conf.Log2File {
		logFile, err = os.OpenFile(conf.LogFilename, os.O_RDWR|os.O_CREATE|os.O_APPEND, 0666)
		if err != nil {
			return fmt.Errorf("failed to open log file %s: %v", conf.LogFilename, err)
		}
		out = logFile
	} else if conf.Log2Stdout {
		out = os.Stdout
	}

	LoggerConf = conf
	DebugLogger = log.New(out, "DIO[DEBUG]: ", log.Ldate|log.Ltime|log.Lshortfile)
	InfoLogger = log.New(out, "DIO[INFO]: ", log.Ldate|log.Ltime)
	WarningLogger = log.New(out, "DIO[WARNING]: ", log.Ldate|log.Ltime)
	ErrorLogger = log.New(out, "DIO[ERROR]: ", log.Ldate|log.Ltime|log.Lshortfile)

	if conf.ProfilingEnabled || conf.SaveTimestamps {
		times = make(map[string]measument)
		if conf.SaveTimestamps {
			timestampsFile, err = os.OpenFile(conf.ProfilingTimesResult, os.O_RDWR|os.O_CREATE, 0666)
		}
	}

	return err
}

func CloseLogger(sessionID string) {
	logFile.Close()
	if LoggerConf.ProfilingEnabled {
		profilingPrintValues(sessionID)
		if LoggerConf.SaveTimestamps {
			timestampsFile.Close()
		}
	}
}

func ProfilingStartMeasurement(name string) {
	if LoggerConf.ProfilingEnabled {
		newMeasurement := measument{}
		newMeasurement.Name = name
		newMeasurement.StartTime = time.Now()
		times[name] = newMeasurement
	}

}

func ProfilingStopMeasurement(name string) {
	if LoggerConf.ProfilingEnabled {
		curMeasurement := times[name]
		curMeasurement.ElapsedTime = time.Since(curMeasurement.StartTime)
		times[name] = curMeasurement
	}
}

func profilingPrintValues(sessionID string) {
	pdata := ProfilingData{}
	pdata.SessionID = sessionID
	pdata.Measuments = make(map[string]time.Duration)
	for k, v := range times {
		pdata.Measuments[k] = v.ElapsedTime
	}
	m, err := JSONMarshalIndent(pdata)
	if err != nil {
		ErrorLogger.Printf("error while saving profiling data: %v\n", err)
	}

	profilingFile, err := os.OpenFile(LoggerConf.ProfilingResult, os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0666)
	if err != nil {
		ErrorLogger.Printf("failed to open profiling result file %s: %v\n", LoggerConf.LogFilename, err)
	}
	profilingFile.Write(m)
	profilingFile.Close()
}

func SaveTimestamp(name string, n_events int, size int) {
	if LoggerConf.SaveTimestamps {
		now := time.Now()      // current local time
		nsec := now.UnixNano() // number of nanoseconds since January 1, 1970 UTC
		str := fmt.Sprintf("%v  %v  %v  %v\n", name, nsec, n_events, size)
		timestampsFile.Write([]byte(str))
	}
}

func SaveKernelTimestamps(calls map[uint64]uint64, submitted map[uint64]uint64, lost map[uint64]uint64) {
	for k, v := range calls {
		ts := Epoch2dateTimeSeconds(k).Unix()
		str := fmt.Sprintf("calls %v %v\n", ts, v)
		timestampsFile.Write([]byte(str))
	}
	for k, v := range submitted {
		ts := Epoch2dateTimeSeconds(k).Unix()
		str := fmt.Sprintf("submitted %v %v\n", ts, v)
		timestampsFile.Write([]byte(str))
	}
	for k, v := range lost {
		ts := Epoch2dateTimeSeconds(k).Unix()
		str := fmt.Sprintf("lost %v %v\n", ts, v)
		timestampsFile.Write([]byte(str))
	}
}

package config

import (
	"fmt"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"

	"github.com/ilyakaznacheev/cleanenv"
)

type TracerConf struct {
	User                 string   `yaml:"user"                    env:"DIO_USER"                    env-description:"User"`
	SessionName          string   `yaml:"session_name"            env:"DIO_SESSION_NAME"            env-description:"Session name"`
	Events               []string `yaml:"events"                  env:"DIO_EVENTS"                  env-description:"Events to trace"`
	TargetPaths          []string `yaml:"target_paths"            env:"DIO_TARGET_PATHS"            env-description:"Paths to trace (everything else is discarded)"`
	TargetPids           []int    `yaml:"target_pids"             env:"DIO_TARGET_PIDS"             env-description:"Process IDs to trace"`
	TargetTids           []int    `yaml:"target_tids"             env:"DIO_TARGET_TIDS"             env-description:"Thread IDs to trace"`
	TargetCommand        string   `yaml:"target_command"          env:"DIO_TARGET_COMMAND"          env-description:"Command to trace (everything else is discarded)"`
	TraceAllProcesses    bool     `yaml:"trace_all_processes"     env:"DIO_TRACE_ALL_PROCESSES"     env-description:"Trace events from all processes"`
	OldChildren          bool     `yaml:"trace_old_processes"     env:"DIO_TRACE_OLD_PROCESSES"     env-description:"Trace events from previously created processes"`
	CaptureProcessEvents bool     `yaml:"capture_proc_events"     env:"DIO_CAPTURE_PROC_EVENTS"     env-description:"Capture process related events"`
	DetailedData         bool     `yaml:"detailed_data"           env:"DIO_DETAILED_DATA"           env-description:"Detailed data"`
	DetailedWithContent  string   `yaml:"detail_with_content"     env:"DIO_DETAIL_WITH_CONTENT"     env-description:"Capture content: 'off', 'plain', 'userspace_hash', 'kernel_hash'"`
	DetailedWithArgPaths bool     `yaml:"detail_with_arg_paths"   env:"DIO_DETAIL_WITH_ARG_PATHS"   env-description:"Capture detailed data with argument paths"`
	DetailedWithSockAddr bool     `yaml:"detail_with_sock_addr"   env:"DIO_DETAIL_WITH_SOCK_ADDR"   env-description:"Capture detailed data with sock addr"`
	DetailedWithSockData bool     `yaml:"detail_with_sock_data"   env:"DIO_DETAIL_WITH_SOCK_DATA"   env-description:"Capture detailed data with sock data"`
	DiscardErrors        bool     `yaml:"discard_errors"          env:"DIO_DISCARD_ERRORS"          env-description:"Discard events with errors"`
	DiscardDirectories   bool     `yaml:"discard_directories"     env:"DIO_DISCARD_DIRS"            env-description:"Discard events operating on directories"`
	MapsStrategy         string   `yaml:"maps_strategy"           env:"DIO_MAPS_STRATEGY"           env-description:"Maps strategy: 'one2many' or 'one2each'"`
	PerfMapSize          int      `yaml:"perfmap_size"            env:"DIO_PERFMAP_SIZE"            env-description:"PerfMap size"`
	Stats                bool     `yaml:"show_stats"              env:"DIO_SHOW_STATS"              env-description:"Show statistics"`
	StatsFile            string   `yaml:"stats_path"              env:"DIO_STATS_PATH"              env-description:"Path to stats result file"`
	Consumers            int      `yaml:"number_consumers"        env:"DIO_NUMBER_CONSUMERS"        env-description:"Number of go-routines sending events to writers"`
	WaitTimeout          int      `yaml:"wait_timeout"            env:"DIO_WAIT_TIMEOUT"            env-description:"Seconds to wait for processing remaining events"`
	Timeout              int      `yaml:"timeout"                 env:"DIO_TIMEOUT"                 env-description:"Stop tracing after timeout (if no events received)"`
	PathsMaxJumps		 int 	  `yaml:"paths_max_jumps"         env:"DIO_PATHS_MAX_JUMPS"         env-description:"Maximum number of jumps (directories) for each file path"`
}

type NopWriterConf struct {
	Enabled bool `yaml:"enabled" env:"DIO_NOPWRITER_ENABLED" env-description:"Enable nop writer"`
}

type FileWriterConf struct {
	Enabled  bool   `yaml:"enabled"   env:"DIO_OUTPUT_FILE_ON"    env-description:"Save events to file"`
	Filename string `yaml:"filename"  env:"DIO_OUTPUT_FILENAME"   env-description:"Output filename"`
	Bulk     bool   `yaml:"bulk"      env:"DIO_OUTPUT_FILE_BULK"  env-description:"Bulk write"`
	BulkSize int    `yaml:"bulk_size" env:"DIO_OUTPUT_FILE_BSIZE" env-description:"Bulk size"`
}
type ElasticsearchWriterConf struct {
	Enabled       bool     `yaml:"enabled"         env:"DIO_OUTPUT_ES_ENABLED"   env-description:"Send events to Elasticsearch"`
	Servers       []string `yaml:"servers"         env:"ES_SERVERS"              env-description:"Elasticsearch servers"`
	Username      string   `yaml:"username"        env:"ES_USERNAME"             env-description:"Elasticsearch username"`
	Password      string   `yaml:"password"        env:"ES_PASSWORD"             env-description:"Elasticsearch password"`
	FlushBytes    int      `yaml:"flush_bytes"     env:"ES_FLUSH_BYTES"          env-description:"Elasticsearch flush bytes"`
	FlushInterval int      `yaml:"flush_interval"  env:"ES_FLUSH_INTERVAL"       env-description:"Elasticsearch flush interval (seconds)"`
}

type OutputConf struct {
	NopWriterConf           `yaml:"nop_writer"`
	FileWriterConf          `yaml:"file_writer"`
	ElasticsearchWriterConf `yaml:"elasticsearch_writer"`
}

type TConfiguration struct {
	TracerConf                `yaml:"tracer"`
	OutputConf                `yaml:"output"`
	utils.LoggerConfiguration `yaml:"logger"`
}

type CliArgs struct {
	ConfigPath    string
	Events        []string
	TargetPids    []int
	TargetTids    []int
	TargetCommand string
	TargetPaths   []string
	User          string
}

func initDefaultConfiguration() TConfiguration {
	cfg := TConfiguration{}

	cfg.TracerConf.User = ""
	cfg.TracerConf.SessionName = ""
	cfg.TracerConf.Events = []string{"all"}
	cfg.TracerConf.TargetPaths = []string{}
	cfg.TracerConf.TargetPids = []int{}
	cfg.TracerConf.TargetTids = []int{}
	cfg.TracerConf.TargetCommand = ""
	cfg.TraceAllProcesses = false
	cfg.TracerConf.OldChildren = false
	cfg.TracerConf.CaptureProcessEvents = false
	cfg.TracerConf.DetailedData = true
	cfg.TracerConf.DetailedWithContent = "off"
	cfg.TracerConf.DetailedWithArgPaths = false
	cfg.TracerConf.DetailedWithSockAddr = false
	cfg.TracerConf.DetailedWithSockData = false

	cfg.TracerConf.DiscardErrors = false
	cfg.TracerConf.DiscardDirectories = false
	cfg.TracerConf.MapsStrategy = "one2each"
	cfg.TracerConf.PerfMapSize = 65536
	cfg.TracerConf.Stats = true
	cfg.TracerConf.StatsFile = ""
	cfg.TracerConf.Consumers = 4
	cfg.TracerConf.WaitTimeout = -1
	cfg.TracerConf.Timeout = -1

	if utils.CheckKernelVersion() {
		cfg.TracerConf.PathsMaxJumps = 45
	} else {
		cfg.TracerConf.PathsMaxJumps = 25
	}

	cfg.NopWriterConf.Enabled = false

	cfg.FileWriterConf.Enabled = true
	cfg.FileWriterConf.Filename = "dio-trace.json"

	cfg.ElasticsearchWriterConf.Enabled = false
	cfg.ElasticsearchWriterConf.Servers = []string{"localhost:9200"}
	cfg.ElasticsearchWriterConf.Username = "elastic"
	cfg.ElasticsearchWriterConf.Password = "password"
	cfg.ElasticsearchWriterConf.FlushBytes = 7000000
	cfg.ElasticsearchWriterConf.FlushInterval = 30

	cfg.LoggerConfiguration = utils.LoggerConfiguration{}
	cfg.LoggerConfiguration.DebugMode = false
	cfg.LoggerConfiguration.ProfilingEnabled = false
	cfg.LoggerConfiguration.ProfilingResult = "dio-profiling.txt"
	cfg.LoggerConfiguration.Log2Stdout = false
	cfg.LoggerConfiguration.Log2File = false
	cfg.LoggerConfiguration.LogFilename = "dio-log.txt"
	cfg.LoggerConfiguration.SaveTimestamps = false
	cfg.LoggerConfiguration.ProfilingTimesResult = "dio-progiling-times.txt"

	return cfg
}

func GetConfiguration(args CliArgs) (TConfiguration, error) {

	cfg := initDefaultConfiguration()

	// read configuration from the file and environment variables
	err := cleanenv.ReadConfig(args.ConfigPath, &cfg)
	if err != nil {
		return cfg, err
	}

	if args.Events != nil {
		cfg.Events = args.Events
	}
	if len(args.TargetPids) > 0 {
		cfg.TargetPids = args.TargetPids
	}
	if len(args.TargetTids) > 0 {
		cfg.TargetTids = args.TargetTids
	}
	if args.TargetCommand != "" {
		if len(args.TargetCommand) > 10 {
			cfg.TargetCommand = args.TargetCommand[0:10]
		} else {
			cfg.TargetCommand = args.TargetCommand
		}
	}
	if args.TargetPaths != nil {
		cfg.TargetPaths = args.TargetPaths
	}
	if args.User != "" {
		cfg.User = args.User
	}

	err = utils.SetupLogger(&cfg.LoggerConfiguration)
	if err != nil {
		return cfg, err
	}

	// check if MapsStrategy is valid
	if cfg.TracerConf.MapsStrategy != "one2many" && cfg.TracerConf.MapsStrategy != "one2each" {
		return cfg, fmt.Errorf("MapsStrategy must be one2many or one2each")
	}

	// check if DetailedWithContent is valid
	if cfg.TracerConf.DetailedWithContent != "off" && cfg.TracerConf.DetailedWithContent != "plain" && cfg.TracerConf.DetailedWithContent != "userspace_hash" && cfg.TracerConf.DetailedWithContent != "kernel_hash" {
		return cfg, fmt.Errorf("invalid value for DetailedWithContent")
	}

	if !cfg.TracerConf.DetailedData {
		print_str := false
		str := "The following configurations will be ignored because 'detailed_data' is disable: "
		if cfg.TracerConf.DetailedWithContent != "off" {
			str += "DetailedWithContent(" + cfg.TracerConf.DetailedWithContent + "); "
			print_str = true
		}
		if cfg.TracerConf.DetailedWithArgPaths {
			str += "DetailedWithArgPaths(true); "
			print_str = true
		}
		if cfg.TracerConf.DetailedWithSockAddr {
			str += "DetailedWithSockAddr(true); "
			print_str = true
		}
		if cfg.TracerConf.DetailedWithSockData {
			str += "DetailedWithSockData(true); "
			print_str = true
		}
		if print_str {
			utils.WarningLogger.Println(str)
		}
	}

	return cfg, err
}

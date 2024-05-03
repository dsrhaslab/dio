package main

import (
	"fmt"
	"os"
	"os/exec"
	"os/signal"
	"os/user"
	"strconv"
	"syscall"
	"time"

	"github.com/dsrhaslab/dio/dio-tracer/pkg/config"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/tracer"
	"github.com/dsrhaslab/dio/dio-tracer/pkg/utils"

	flag "github.com/spf13/pflag"
)

var Version = "development"

// convert types take an int and return a string value.
type callback func()

func checkError(err error) {
	if err != nil {
		fmt.Errorf("Error:", err.Error())
		os.Exit(2)
	}
}

func checkErrorWithCallback(err error, fn callback) {
	if err != nil {
		fmt.Println(err.Error())
		fn()
		os.Exit(2)
	}
}

func waitForProcess(pid int) {

	pr, err := os.FindProcess(pid)
	checkError(err)

	ps, err := pr.Wait()
	checkError(err)
	utils.InfoLogger.Printf("Process %v exited with code: %v\n", pid, ps.ExitCode())
}

func sigHandler(stopChan chan bool) {
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGQUIT, syscall.SIGTERM)
	<-sig
	stopChan <- true
	signal.Stop(sig)
	signal.Reset()
}

func sigHandlerPid(pid int) {
	sig := make(chan os.Signal, 1)

	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)

	for {
		s := <-sig

		pgid, err := syscall.Getpgid(pid)
		checkError(err)

		switch s {
		case syscall.SIGINT:
			err = syscall.Kill(-pgid, syscall.SIGINT)
		case syscall.SIGTERM:
			err = syscall.Kill(-pgid, syscall.SIGTERM)
		case syscall.SIGQUIT:
			err = syscall.Kill(-pgid, syscall.SIGQUIT)
		}

		if err != nil {
			break
		}
	}
	signal.Stop(sig)
}

func startTargetProgram(target_pid int, stopChan chan bool) {

	// signal target process to continue
	syscall.Kill(target_pid, syscall.SIGCONT)
	utils.ProfilingStartMeasurement("target_program_execution")
	// wait for target process to finish
	waitForProcess(target_pid)
	utils.ProfilingStopMeasurement("target_program_execution")
	signal.Reset()
	stopChan <- true
}

func start_tracer_by_pid(conf config.TConfiguration) {
	// ----- PREPARE TRACER -----
	btracer, err := tracer.InitTracer(&conf, true)
	checkError(err)

	go sigHandler(btracer.StopChan)

	// ----- START TRACER -----
	err = btracer.Run()
	checkError(err)
	btracer.Close()
}

func start_target_program(args []string) {
	var cmdArgs = args[0 : len(args)-1]
	var cmdName = args[0]

	cmd, err := exec.LookPath(cmdName)
	checkErrorWithCallback(err, func() {
		syscall.Kill(-syscall.Getppid(), syscall.SIGTERM)
	})

	// Set up channel on which to send signal notifications.
	// We must use a buffered channel or risk missing the signal
	// if we're not ready to receive when the signal is sent.
	c := make(chan os.Signal, 1)
	signal.Notify(c, syscall.SIGCONT)

	// Block until a signal is received.
	<-c
	signal.Stop(c)

	// Starting target program
	err = syscall.Exec(cmd, cmdArgs, os.Environ())
	checkError(err)
}

func start_tracer_for_child_process(target_pid int, conf config.TConfiguration, command []string) {

	// ----- PREPARE TRACER -----
	conf.TracerConf.TargetPids = make([]int, 1)
	conf.TracerConf.TargetPids[0] = target_pid
	utils.InfoLogger.Println("Target command: ", command)
	btracer, err := tracer.InitTracer(&conf, false)
	checkError(err)

	go sigHandlerPid(target_pid)

	// ----- START TRACER -----
	err = btracer.Run()
	checkError(err)
	time.Sleep(5 * time.Second)
	go startTargetProgram(target_pid, btracer.StopChan)

	btracer.Close()
}

func start_tracer_by_command(conf config.TConfiguration, args []string) {
	// check args length
	if len(args) <= 0 {
		checkError(fmt.Errorf("At leats a PID or a COMMAND must be passed!"))
	}
	command := args

	if _, isChild := os.LookupEnv("CHILD_ID"); !isChild {
		args := append(os.Args, fmt.Sprintf("#child_%d", 1))
		childENV := []string{"CHILD_ID=1"}
		pwd, err := os.Getwd()
		if err != nil {
			utils.ErrorLogger.Fatalf("getwd err: %s", err)
		}
		var procAttr *syscall.SysProcAttr
		if conf.User != "" {
			u, err := user.Lookup(conf.User)
			checkError(err)

			uid, err := strconv.ParseInt(u.Uid, 10, 32)
			checkError(err)

			gid, err := strconv.ParseInt(u.Gid, 10, 32)
			checkError(err)

			procAttr = &syscall.SysProcAttr{
				Setsid:     true,
				Credential: &syscall.Credential{Uid: uint32(uid), Gid: uint32(gid)},
			}
		} else {
			procAttr = &syscall.SysProcAttr{
				Setsid: true,
			}
		}

		childPID, _ := syscall.ForkExec(args[0], args, &syscall.ProcAttr{
			Dir:   pwd,
			Env:   append(os.Environ(), childENV...),
			Sys:   procAttr,
			Files: []uintptr{0, 1, 2},
		})

		start_tracer_for_child_process(childPID, conf, command)
	} else {
		start_target_program(command)
	}
}

func main() {

	flags := config.CliArgs{}

	flagSet := flag.NewFlagSet("dio", flag.ContinueOnError)
	flagSet.StringVar(&flags.ConfigPath, "config", "/usr/share/dio/conf/config.yaml", "Path to configuration file")
	flagSet.StringSliceVar(&flags.Events, "events", nil, "Events to trace (separated by comma)")
	flagSet.StringSliceVar(&flags.TargetPaths, "target_paths", nil, "Paths to trace (separated by comma)")
	flagSet.IntSliceVar(&flags.TargetPids, "pid", nil, "PIDs to trace (separated by comma)")
	flagSet.IntSliceVar(&flags.TargetTids, "tid", nil, "TIDs to trace (separated by comma)")
	flagSet.StringVar(&flags.TargetCommand, "comm", "", "Command to trace")
	flagSet.StringVar(&flags.User, "user", "", "Run program as a specific user")
	version := flagSet.Bool("version", false, "Prints current DIO-tracer version")

	os_args := os.Args
	err := flagSet.Parse(os_args[1:])
	checkError(err)

	if *version {
		fmt.Println(Version)
		os.Exit(0)
	}

	args := flagSet.Args()

	conf, err := config.GetConfiguration(flags)
	checkError(err)

	// check if current process is the child process
	if os_args[len(os_args)-1] == "#child_1" {
		start_target_program(args)
	} else {

		if len(args) > 0 {
			start_tracer_by_command(conf, args)
		} else if len(conf.TracerConf.TargetPids) > 0 || len(conf.TracerConf.TargetTids) > 0 {
			// Start tracing by pid
			start_tracer_by_pid(conf)
		} else {
			conf.TraceAllProcesses = true
			start_tracer_by_pid(conf)
		}
	}
}

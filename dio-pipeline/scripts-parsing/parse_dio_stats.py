#
# Script for parsing events' statistics from DIO
# Run as follow: $ python3 parse_dio_stats.py <path to results dir>
# The path to results dir refer to the folder containing the different types of tests.
# The script will serach for subdirectories (e.g., 'run_1', 'run_2', etc.) and parse the 'dio-stats.json' file inside each subdirectory.
import sys
import os
import csv
import commons

def parseSetup(path):
    runs_data = dict()
    runs = commons.ListDir(path)

    # get data for each run
    for run in runs:
        run_p_file = path + "/" + run + "/dio-stats.json"
        print("++++++ Parsing file '{0}'".format(run_p_file))
        data = commons.GetJsonData(run_p_file)

        event_type_info = None
        if "path" in data["tracer_stats"]:
            event_type_info = data["tracer_stats"]["path"]

        process_info = None
        if "process" in data["tracer_stats"]:
            process_info = data["tracer_stats"]["process"]

        # for event_type in data["bpf_stats"]:
        #     if event_type["event"] == "event_path":
        #         print("Event paths calls: {0}".format(event_type["calls"]))
        #         event_type_info = event_type
        #         break

        for m in data["tracer_stats_total"]:
            if not m in runs_data:
                runs_data[m] = []

            if m == "calls" or m == "returned" or m == "lost" or m == "incomplete" or m == "saved":
                subtract = 0
                if event_type_info != None and m in event_type_info:
                    if m+"_event_path" not in runs_data:
                        runs_data[m+"_event_path"] = []
                    runs_data[m+"_event_path"].append(event_type_info[m])
                    subtract += event_type_info[m]
                    print("Event paths {0}: {1}".format(m, event_type_info[m]))
                if process_info != None and m in process_info:
                    if m+"_process" not in runs_data:
                        runs_data[m+"_process"] = []
                    runs_data[m+"_process"].append(process_info[m])
                    subtract += process_info[m]
                    print("Process {0}: {1}".format(m, process_info[m]))
                runs_data[m].append(data["tracer_stats_total"][m] - subtract)
                print("{0} val: {1}, sub: {2}".format(m, data["tracer_stats_total"][m], subtract))
            else:
                runs_data[m].append(data["tracer_stats_total"][m])


    print(runs_data)

    # replace list of values by its average and standard deviation
    for cur in runs_data:
        print(cur)
        values = runs_data[cur]
        avg = commons.Average(values)
        dev = commons.STDev(values)
        runs_data[cur] = (avg, dev)

    return runs_data

def parseAll(input_dir):
    setups_dir = commons.ListDir(input_dir)
    all_data_dic = dict()
    header = ["events"]
    processed_dirs = 0

    for dir in setups_dir:
        print("\n==> Parsing DIO stats results for test '{0}'.".format(dir))

        try:
            data = parseSetup(input_dir+"/"+dir)
        except EnvironmentError:
            continue

        for key in data:
            if not key in all_data_dic:
                all_data_dic[key] = [0] * processed_dirs
            all_data_dic[key].append(data[key])

        for key in all_data_dic:
            if key not in data:
                all_data_dic[key].append((0,0))

        header.append(dir+"-AVG")
        header.append(dir+"-DEV")
        processed_dirs += 1
        input("Press Enter to continue...")

    return header, all_data_dic

def storeCSV(header, data, output_file):
    print("storeCSV", header, data)
    with open(output_file, 'w', encoding='UTF8', newline='') as f:
        writer = csv.writer(f, delimiter=";")
        writer.writerow(header)
        for row in data:
            cur_data = [row]
            for val in data[row]:
                cur_data.append(val[0])
                cur_data.append(val[1])
            writer.writerow(commons.LocalizeFloats(cur_data))

def main():
    if (len(sys.argv)) <= 1:
        print("Script requires the path to results folder")
        exit(1)

    try:
        args = sys.argv[1:]
        input_dir = args[0]
        print("> Parsing DIO Stats for folder '{0}'.".format(input_dir))

        output_file = os.path.basename(os.path.normpath(input_dir)) + "_stats.csv"

        header, data = parseAll(input_dir)
        storeCSV(header, data, output_file)
        print("\n> Results saved to file '{0}'.".format(output_file))

    except Exception as e:
        print("Error [{0}]: {1}".format(e.__traceback__.tb_lineno, e))


if __name__ == "__main__":
    main()

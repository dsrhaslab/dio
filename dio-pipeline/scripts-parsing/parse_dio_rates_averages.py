#
# Script for computing DIO rates
# Run as follow: $ python3 parse_dio_rates.py <path to results dir>
# The path to results dir refer to the folder containing the different types of tests.
# The script will search for subdirectories (e.g., 'run_1', 'run_2', etc.) and parse the 'dio-profiling-times.json' file inside each subdirectory.
import sys
import os
import re
import commons
from tqdm import tqdm


def parseSetup(path):
    runs = commons.ListDir(path)

    total_call_rate = []
    total_submit_rate = []
    total_lost_rate = []

    # get data for each run
    for run in runs:
        run_p_file = path + "/" + run + "/dio-profiling-times.json"
        print("++++++ Parsing file '{0}'".format(run_p_file))

        run_call_bytes = 0
        run_call_nr = 0
        run_submit_bytes = 0
        run_submit_nr = 0
        run_lost_bytes = 0
        run_lost_nr = 0
        with open(run_p_file) as file:
            for line in tqdm(file, total=commons.GetNumLines(run_p_file)):
                if re.search("call", line):
                    run_call_nr += 1
                    _, _, bytes = line.split(" ")
                    run_call_bytes += int(bytes)
                elif re.search("submit", line):
                    run_submit_nr += 1
                    _, _, bytes = line.split(" ")
                    run_submit_bytes += int(bytes)
                elif re.search("lost", line):
                    run_lost_nr += 1
                    _, _, bytes = line.split(" ")
                    run_lost_bytes += int(bytes)
        call_rate = run_call_bytes / run_call_nr /1024/1024
        total_call_rate.append(call_rate)
        submit_rate = run_submit_bytes / run_submit_nr /1024/1024
        total_submit_rate.append(submit_rate)
        lost_rate = run_lost_bytes / run_lost_nr /1024/1024
        total_lost_rate.append(lost_rate)

    res_data = {}
    res_data["call_rate"] = {}
    res_data["call_rate"]["avg"] = commons.Average(total_call_rate)
    res_data["call_rate"]["dev"] = commons.STDev(total_call_rate)
    res_data["submit_rate"] = {}
    res_data["submit_rate"]["avg"] = commons.Average(total_submit_rate)
    res_data["submit_rate"]["dev"] = commons.STDev(total_submit_rate)
    res_data["lost_rate"] = {}
    res_data["lost_rate"]["avg"] = commons.Average(total_lost_rate)
    res_data["lost_rate"]["dev"] = commons.STDev(total_lost_rate)

    return res_data

def parseAll (input_dir):
    out_dirs = commons.ListDir(input_dir)
    all_data_dic = dict()
    all_data_dic["calls"] = dict()
    all_data_dic["submitted"] = dict()
    all_data_dic["lost"] = dict()
    processed_dirs = 0
    for dir in out_dirs:
        run_data = parseSetup(input_dir+"/"+dir)
        all_data_dic["calls"][dir] = run_data["call_rate"]
        all_data_dic["submitted"][dir] = run_data["submit_rate"]
        all_data_dic["lost"][dir] = run_data["lost_rate"]
        processed_dirs += 1
    return all_data_dic


def prepareDAT(data):
    # prepare data for DAT format
    print("Preparing data for dat file.")
    dat_data = {}
    stor_list = []
    for param in data:
        dat_data[param] = {}

        for dir in data[param]:
            if "1ES" in dir:
                stor = "elk"
                setup = dir.replace("dio_1ES_profiling_","")
            elif "file" in dir:
                stor = "file"
                setup = dir.replace("dio_file_profiling_","")
            elif "null" in dir:
                stor = "null"
                setup = dir.replace("dio_null_profiling_","")
            else:
                stor = "unknown"
                setup = dir
                print("unknown storage type:", dir)

            if stor not in stor_list:
                stor_list.append(stor)
            if setup not in dat_data[param]:
                dat_data[param][setup] = {}
            dat_data[param][setup][stor] = data[param][dir]

    return stor_list, dat_data

def storeDAT(data, output_file):
    stor_list, dat_data = prepareDAT(data)

    header = "setup;"
    for stor in stor_list:
        header += stor + ";" + stor + "-DEV;"

    print(dat_data)
    with open(output_file, 'w',  newline='') as f:
        for param in dat_data:
            print("#{0};".format(param), file=f)
            print(header, file=f)
            for setup in dat_data[param]:
                line = "{0};".format(setup)
                for stor in stor_list:
                    line += "{0};{1};".format(dat_data[param][setup][stor]["avg"], dat_data[param][setup][stor]["dev"])
                print(line.replace("_", "\\\\\\_"), file=f)
            print("\n", file=f)

def main():
    if (len(sys.argv)) <= 1:
        print("Script requires the path to results folder")
        exit(1)

    try:
        args = sys.argv[1:]
        input_dir = args[0]
        print("> Parsing Filebench results for folder '{0}'.".format(input_dir))

        output_file = os.path.basename(os.path.normpath(args[0])) + "_dio_rates.dat"

        data = parseAll(input_dir)
        storeDAT(data, output_file)
        print("\n> Results saved to file '{0}'.".format(output_file))

    except Exception as e:
        print("Error: {0}".format(e))


if __name__ == "__main__":
    main()

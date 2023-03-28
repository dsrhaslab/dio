#
# Script for parsing profiling results file from DIO
# Run as follow: $ python3 parse_profiling.py <path to results dir>
# The path to results dir refer to the folder containing the different types of tests.
# The script will search for subdirectories (e.g., 'run_1', 'run_2', etc.) and parse the 'dio-profiling.json' file inside each subdirectory.
from email import header
from pprint import pprint
import sys
import os
import csv
import collections
import commons

def parseSetup(path):
    runs_data = dict()
    runs = commons.ListDir(path)

    # get data for each run
    for run in runs:
        run_p_file = path + "/" + run + "/dio-profiling.json"
        print("++++++ Parsing file '{0}'".format(run_p_file))
        data = commons.GetJsonData(run_p_file)

        for m in data["profiling_data"]:
            if not m in runs_data:
                runs_data[m] = []
            runs_data[m].append(data["profiling_data"][m])

    # replace list of values by its average
    for cur in runs_data:
        values = runs_data[cur]
        avg = commons.Average(values) / 1000000
        dev = commons.STDev(values) / 1000000
        runs_data[cur] = (avg, dev)

    # sort dictionary
    runs_data = collections.OrderedDict(sorted(runs_data.items()))
    return runs_data

def parseAll(input_dir):
    setup_dirs = commons.ListDir(input_dir)
    all_data_dic = dict()
    header = []
    processed_dirs = 0

    for dir in setup_dirs:
        print("\n==> Parsing profiling results for test '{0}'.".format(dir))

        try:
            data = parseSetup(input_dir+"/"+dir)
        except EnvironmentError:
            continue

        for key in data:
            if not key in all_data_dic:
                all_data_dic[key] = dict()

            if not dir in all_data_dic[key]:
                all_data_dic[key][dir] = dict()

            all_data_dic[key][dir]["AVG"] = data[key][0]
            all_data_dic[key][dir]["DEV"] = data[key][1]

        header.append(dir)
        processed_dirs += 1

    return header, all_data_dic

def storeCSV(setups, data, output_file):
    with open(output_file, 'w', encoding='UTF8', newline='') as f:
        writer = csv.writer(f, delimiter=";")

        header = ["measure (ms)"]
        for setup in setups:
            header.append(setup)
            header.append(setup+"-DEV")
        writer.writerow(header)
        pprint(data)

        for param in data:
            row = [param]

            for setup in sorted(setups):
                if setup == "measure (ms)":
                    continue
                if setup in data[param]:
                    row.append(data[param][setup]["AVG"])
                    row.append(data[param][setup]["DEV"])
                else:
                    row.append(0)
                    row.append(0)
            print(row)
            writer.writerow(commons.LocalizeFloats(row))

def main():
    if (len(sys.argv)) <= 1:
        print("Script requires the path to results folder")
        exit(1)

    try:
        args = sys.argv[1:]
        input_dir = args[0]
        print("> Parsing Filebench results for folder '{0}'.".format(input_dir))

        output_file = os.path.basename(os.path.normpath(args[0])) + "_profiling.csv"

        header, data = parseAll(input_dir)
        storeCSV(header, data, output_file)
        print("\n> Results saved to file '{0}'.".format(output_file))

    except Exception as e:
        print("Error: {0}".format(e))

if __name__ == "__main__":
    main()

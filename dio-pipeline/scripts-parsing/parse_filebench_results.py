#
# Script for parsing Filebench results
# Run as follow: $ python3 parse_filebench_results.py <path to results dir>
# The path to results dir refer to the folder containing the different types of tests.
# The script will search for subdirectories (e.g., 'run_1', 'run_2', etc.) and parse the 'filebench_output.txt' (or 'docker_logs.txt') file inside each subdirectory.
import sys
import os
import csv
import commons
from pathlib import Path

def parseSetup(path):
    runs_data = dict()
    runs = commons.ListDir(path)

    # get data for each run
    for run in runs:
        run_p_file = path + "/" + run + "/filebench_output.txt"
        print("++++++ Parsing file '{0}'".format(run_p_file))
        content = []

        results_file = Path(run_p_file)
        if results_file.is_file():
            with open(run_p_file) as f:
                for ln in f:
                    if ln.__contains__("IO Summary"):
                        content = ln.split(" ")
                        break
            if len(content) == 0:
                print("Could not find IO Summary in file '{0}'".format(run_p_file))
                continue
        else :
            run_p_file = path + "/" + run + "/docker_logs.txt"
            with open(run_p_file) as f:
                for ln in f:
                    if ln.__contains__("IO Summary"):
                        content = ln.split(" ")
                        break
            if len(content) == 0:
                print("Could not find IO Summary in file '{0}'".format(run_p_file))
                continue

        run_values = dict()
        run_values["operations"] = int(content[3])
        run_values["operations_per_sec"] = float(content[5])
        rd_wr = content[7].split("/")
        run_values["rd"] = int(rd_wr[0])
        run_values["wr"] = int(rd_wr[1])
        run_values["mb_per_sec"] = float(content[9].replace("mb/s",""))
        run_values["ms_per_op"] = float(content[10].replace("ms/op",""))

        for m in run_values:
            if not m in runs_data:
                runs_data[m] = []
            runs_data[m].append(run_values[m])

    # replace list of values by its average and standard deviation
    for cur in runs_data:
        values = runs_data[cur]
        avg = commons.Average(values)
        dev = commons.STDev (values)
        runs_data[cur] = (avg, dev)

    return runs_data

def parseAll(input_dir):
    setups_dirs = commons.ListDir(input_dir)
    all_data_dic = dict()
    header = ["param"]
    processed_dirs = 0

    for dir in setups_dirs:
        print("\n==> Parsing filebench results for test '{0}'.".format(dir))

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

    return header, all_data_dic

def storeCSV(header, data, output_file):
    with open(output_file, 'w', encoding='UTF8', newline='') as f:
        writer = csv.writer(f, delimiter=';')
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
        print("> Parsing Filebench results for folder '{0}'.".format(input_dir))

        output_file = os.path.basename(os.path.normpath(input_dir)) + "_filebench.csv"

        header, data = parseAll(input_dir)
        storeCSV(header, data, output_file)
        print("\n> Results saved to file '{0}'.".format(output_file))

    except Exception as e:
        print("Error: {0}".format(e))

if __name__ == "__main__":
    main()

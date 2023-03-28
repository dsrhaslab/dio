#
# Script for parsing dstat CSV file
# Run as follow: $ python3 parse_dstat.py <path to results dir>
# The path to results dir refer to the folder containing the different types of tests.
# The script will search for subdirectories (e.g., 'run_1', 'run_2', etc.) and parse the 'dstat.csv' file inside each subdirectory.

import sys
import os
import csv
import commons


def parseSetup(path):
    runs = commons.ListDir(path)
    all_runs = dict()

    # get data for each run
    for run in runs:
        run_p_file = path + "/" + run + "/dstat.csv"
        print("++++++ Parsing file '{0}'".format(run_p_file))

        with open(run_p_file) as f:
            reader = csv.reader(f)

            start = 0
            r = 0
            header = []
            run_values = dict()
            for row in reader:
                r+=1

                # ignore empty lines
                if (len(row) == 0): continue

                # ignore lines started with "Dstat" or "Author"
                if row[0].startswith("Dstat") or row[0].startswith("Author:"):
                    continue

                # if a new line with "Host" is found, restart
                if row[0].startswith("Host:"):
                    if (r != 3):
                        print("Found a new \"Host\" line at row {0}. Restarting...".format(r))
                    start = r
                    run_values = dict()
                    header = []

                # save components list
                if (r == start+2):
                    comp = row
                    continue

                # concat each component with its parameters
                if (r == start+3):
                    cur_param_index = 0
                    last_pos = 0
                    for i in range(len(comp)):
                        if (last_pos >= len(comp)): continue
                        cur_val = comp[last_pos]
                        num_param = 1

                        while((last_pos+1 < len(comp)) and comp[last_pos+1]==""):
                            last_pos+=1
                            num_param+=1

                        for j in range(num_param):
                            cur_param = cur_val.replace(" ", "_") + "__" + row[cur_param_index]
                            header.append(cur_param)
                            j+=1
                            cur_param_index+=1
                        last_pos += 1
                    continue

                # Parse values
                if (r>start+3):
                    if (len(row) < 16):
                        print(r, row)
                        continue

                    for index in range(len(row)-1):
                        # ignore time column
                        if "time" in header[index]:
                            continue

                        if not header[index] in run_values:
                            run_values[header[index]] = []
                        run_values[header[index]].append(float(row[index]))

            # Compute averages
            for cur in run_values:
                if not cur in all_runs:
                    all_runs[cur] = []
                avg = commons.Average(run_values[cur])
                all_runs[cur].append(avg)

    # replace list of values by its average
    for cur in all_runs:
        values = all_runs[cur]
        avg = commons.Average(values)
        dev = commons.STDev(values)
        all_runs[cur] = (avg, dev)

    return all_runs

def parseAll(input_dir):
    setups_dir = commons.ListDir(input_dir)
    all_data_dic = dict()
    header = ["param"]
    processed_dirs = 0

    for dir in setups_dir:
        print("\n==> Parsing Dstat results for test '{0}'.".format(dir))

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

        output_file = os.path.basename(os.path.normpath(args[0])) + "_dstat.csv"

        header, data = parseAll(input_dir)
        storeCSV(header, data, output_file)
        print("\n> Results saved to file '{0}'.".format(output_file))

    except Exception as e:
        print("Error: {0}".format(e))

if __name__ == "__main__":
    main()

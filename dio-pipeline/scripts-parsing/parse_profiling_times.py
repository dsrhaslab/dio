#
# Script for parsing profiling times results file from DIO
# Run as follow: $ python3 parse_profiling.py <path to dio-profiling.json file>
from os import mkdir
import sys
import math
from tqdm import tqdm
import commons

def parse(path):
    runs_data = dict()

    with open(path) as file:
        for line in tqdm(file, total=commons.GetNumLines(path)):
            data = line.split("  ",4)
            if len(data) < 3:
                continue
            second = math.ceil(int(data[1]) / 1000000000)
            if data[0] not in runs_data:
                runs_data[data[0]] = dict()

            if second not in runs_data[data[0]]:
                runs_data[data[0]][second] = (0,0)
            cur_d = runs_data[data[0]][second]
            runs_data[data[0]][second] = (cur_d[0] + int(data[2]), cur_d[1] + int(data[3]))

    return runs_data

def storeDAT(data, output_dir):
    commons.CreateDir(output_dir)

    for str in data:
        output_file = output_dir + "/" + str + ".dat"
        with open(output_file, 'w', encoding='UTF8', newline='') as f:
            for sec in data[str]:
                dt_object = sec
                f.write("{0};{1}\n".format(dt_object, int(data[str][sec][0])))
        print("++++++ created file '{0}'".format(output_file))


def main():
    if (len(sys.argv)) <= 1:
        print("Script requires the path to results folder")
        exit(1)

    try:
        input_file = sys.argv[1]
        output_dir = "profiling_times"
        print("> Parsing DIO profiling times for file '{0}'.".format(input_file))

        data = parse(input_file)
        storeDAT(data, output_dir)

        print("\n> Results saved to directory '{0}'.".format(output_dir))

    except Exception as e:
        print("Error: {0}".format(e))

if __name__ == "__main__":
    main()
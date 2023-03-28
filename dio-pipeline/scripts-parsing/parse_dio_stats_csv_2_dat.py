import sys
import csv
import pprint

def parseCSV(path):
    data = dict()
    list_params = []
    cur_param = ""
    reader = csv.DictReader(open(path), delimiter=';')
    while True:
        try:
            dictobj = next(reader)
            for key in dictobj:
                if (key == 'events'):
                    cur_param = dictobj[key]
                    continue
                setup = key
                if cur_param not in list_params:
                    list_params.append(cur_param)
                if ("AVG" in setup):
                    setup = key.replace("-AVG","")
                    VAL = "AVG"
                elif ("DEV" in setup):
                    setup = key.replace("-DEV","")
                    VAL = "DEV"
                if (setup not in data):
                    data[setup] = {}
                if cur_param not in data[setup]:
                    data[setup][cur_param] = {}
                data[setup][cur_param][VAL] = dictobj[key]
        except StopIteration:
            break
    return list_params, data

def storeDAT(list_params, data, path):
    with open(path, 'w',  newline='') as f:
        header = "stats;"
        data_lines = []

        for param in sorted(list_params):
            header += " {0}; {0}-DEV;".format(param)
        print(header.replace("_", "\\\\\\_"), file=f)

        for setup in data:
            line = "{0};".format(setup)
            for param in sorted(list_params):
                line += " {0}; {1};".format(data[setup][param]["AVG"], data[setup][param]["DEV"]).replace(",",".")
            data_lines.append(line)

        for line in data_lines:
            print(line.replace("_", "\\\\\\_"), file=f)

def main():
    if (len(sys.argv)) <= 1:
        print("Script requires the path to the DIO stats CSV file")
        exit(1)

    input_file = sys.argv[1]
    output_file = input_file + ".dat"

    try:
        print("> Parsing CSV file '{0}'.\n".format(input_file))
        list_params, data = parseCSV(input_file)
        pprint.pprint(data)
        storeDAT(list_params, data, output_file)
        print("\n> Results saved to file '{0}'.".format(output_file))
    except Exception as e:
        print("ERROR: {0}".format(e))

if __name__ == "__main__":
    main()
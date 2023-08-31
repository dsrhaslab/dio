import sys
import csv
import pprint

def parseCSV(path):
    data = dict()
    cur_param = ""
    reader = csv.DictReader(open(path), delimiter=';')
    while True:
        try:
            dictobj = next(reader)

            for key in dictobj:
                if (key == 'param'):
                    cur_param = dictobj[key]
                    if (cur_param not in data):
                        data[cur_param] = {}
                    continue

                setup = key
                if ("AVG" in setup):
                    setup = key.replace("-AVG","")
                    VAL = "AVG"
                elif ("DEV" in setup):
                    setup = key.replace("-DEV","")
                    VAL = "DEV"

                if setup not in data[cur_param]:
                    data[cur_param][setup] = {}
                data[cur_param][setup][VAL] = dictobj[key]
        except StopIteration:
            break
    return data

def storeDAT(data, path):
    with open(path, 'w',  newline='') as f:
        for key in data:
            print("#{0}; AVG; DEV;".format(key).replace("_", "\\\\\\_"), file=f)
            for setup in data[key]:
                line = "{0}; {1}; {2};".format(setup, data[key][setup]["AVG"], data[key][setup]["DEV"]).replace(",",".").replace("_", "\\\\\\_")
                print(line, file=f)
            print("\n", file=f)

def main():
    if (len(sys.argv)) <= 1:
        print("Script requires the path to the Filebench CSV file")
        exit(1)

    try:
        input_file = sys.argv[1]
        output_file = input_file + ".dat"

        print("> Parsing CSV file '{0}'.\n".format(input_file))
        data = parseCSV(input_file)
        pprint.pprint(data)
        storeDAT(data, output_file)
        print("\n> Results saved to file '{0}'.".format(output_file))

    except Exception as e:
        print("ERROR: {0}".format(e))

if __name__ == "__main__":
    main()
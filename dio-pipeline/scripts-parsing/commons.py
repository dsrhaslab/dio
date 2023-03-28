import os
import json
from statistics import mean, stdev
from subprocess import check_output


def ListDir(path):
    return sorted(os.listdir(path))

def Average(lst):
    if len(lst) == 0:
        return 0
    return float(round(mean(lst), 3))

def STDev(lst):
    if (len(lst) > 1):
        return float(round(stdev(lst), 3))
    else:
        return 0

def LocalizeFloats(row):
    return [
        str(el).replace('.', ',') if isinstance(el, float) else el
        for el in row
    ]

def GetJsonData(path):
    with open(path) as json_file:
        data = json.load(json_file)
    return data


def GetNumLines(file_path):
    return int(check_output(["wc", "-l", file_path]).split()[0])


def CreateDir(path):
    if not os.path.exists(path):
        os.makedirs(path)
#!/bin/bash

RES_DIR="profiling_times_r$2"

mkdir -p $RES_DIR

for entry in `ls $1`; do
    echo $entry

    echo "# events called" >> "profiling_times_r$2/$entry.dat"

    cat "$1/$entry/run_$2/dio-profiling-times.json" | grep calls | wc -l
    cat "$1/$entry/run_$2/dio-profiling-times.json" | grep calls | cut -d ' ' -f 2,3 | sort >> "profiling_times_r$2/$entry.dat"

    echo -e "\n\n# events submitted" >> "profiling_times_r$2/$entry.dat"
    cat "$1/$entry/run_$2/dio-profiling-times.json" | grep submitted | wc -l
    cat "$1/$entry/run_$2/dio-profiling-times.json" | grep submitted | cut -d ' ' -f 2,3 | sort >> "profiling_times_r$2/$entry.dat"

    echo -e "\n\n# events lost" >> "profiling_times_r$2/$entry.dat"
    cat "$1/$entry/run_$2/dio-profiling-times.json" | grep lost | wc -l
    cat "$1/$entry/run_$2/dio-profiling-times.json" | grep lost | cut -d ' ' -f 2,3 | sort >> "profiling_times_r$2/$entry.dat"

done
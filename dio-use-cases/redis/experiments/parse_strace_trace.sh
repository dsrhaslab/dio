#!/bin/bash

# Set variables
INPUT_FILE="strace.out"
OUTPUT_FILE=$(pwd)/strace_trace_stats.txt

############################################################
# Help                                                     #
############################################################
Help()
{
   # Display Help
   echo "Get strace events stats."
   echo
   echo "Syntax: get_strace_events_stats.sh [-i|-o|h]"
   echo "options:"
   echo "-i     Input file (default: ${INPUT_FILE})"
   echo "-o     Output file (default: ${OUTPUT_FILE})"
   echo "-h     Print this Help."
   echo
}

############################################################
############################################################
# Main program                                             #
############################################################
############################################################



function count_unfinished_syscalls {
    res=0
    res=$( cat $INPUT_FILE | grep "unfinished" | wc -l )
}

function count_resumed_syscalls {
    res=0
    res=$( cat $INPUT_FILE | grep "resumed" | wc -l )
}

function count_finished_syscalls {
    res=0
    res=$( cat $INPUT_FILE | grep -v "resumed" | grep -v "unfinished" | grep -v "\-\-\- SIG" | grep -v "+++ exit" | wc -l )
}

function get_unfinished_syscalls {
    cat $INPUT_FILE | grep "unfinished" | cut -f3 -d' ' | cut -f1 -d'(' | sort | uniq -c
}

function get_resumed_syscalls {
    cat $INPUT_FILE | grep "resumed" | cut -f4 -d' ' | sort | uniq -c
}

function get_finished_syscalls {
    cat $INPUT_FILE | grep -v "resumed" | grep -v "unfinished" | grep -v "\-\-\- SIG" | grep -v "+++ exit" |  cut -f3 -d' ' | cut -f1 -d'(' | sort | uniq -c
}

function get_stats {
    : > $OUTPUT_FILE

    count_unfinished_syscalls
    unfinished=$res

    count_resumed_syscalls
    resumed=$res

    count_finished_syscalls
    finished=$res

    total=$((unfinished/2 + resumed/2 + finished))

    if [ "$unfinished" != "0" ]; then
        if [ "$unfinished" != "$resumed" ]; then
            echo "WARNING: unfinished syscalls ($unfinished) != resumed syscalls ($resumed)" >> $OUTPUT_FILE
        fi

        echo -e "\n---Unfinished syscalls---" >> $OUTPUT_FILE
        get_resumed_syscalls >> $OUTPUT_FILE

        echo -e "\n---Resumed syscalls---" >> $OUTPUT_FILE
        get_unfinished_syscalls >> $OUTPUT_FILE
    fi

    echo -e "\n---Finished syscalls---" >> $OUTPUT_FILE
    get_finished_syscalls >> $OUTPUT_FILE

    echo -e "\nUnfinished syscalls: "$((unfinished/2))" (of $unfinished)" >> $OUTPUT_FILE
    echo -e "Resumed syscalls: "$((resumed/2))" (of $resumed)" >> $OUTPUT_FILE
    echo -e "Finished syscalls: $finished" >> $OUTPUT_FILE
    echo -e "Total syscalls: $total" >> $OUTPUT_FILE
}


############################################################
# Process the input options. Add options as needed.        #
############################################################
# Get the options
while getopts ":hi:o:" option; do
    case $option in
        h) # display Help
            Help
            exit;;
        i) INPUT_FILE=${OPTARG};;
        o) OUTPUT_FILE=${OPTARG};;
        \?) # Invalid option
            echo "Error: Invalid option '${OPTARG}'"
            exit;;
    esac
done

echo "$(date) | Getting strace stats..."
echo "$(date) | Input file: ${INPUT_FILE}"
echo "$(date) | Output file: ${OUTPUT_FILE}"

get_stats

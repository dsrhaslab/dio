#!/bin/bash

ES_USERNAME="dio"
ES_PASSWORD="diopw"
ES_SERVERS="localhost:9200"
SLEEP=0s
N_TRIES=3
WAITING=false
TIME_WAITING_FOR_INDEX=30s

echo $$ > /usr/share/dio/fpca.pid
echo $$

# $1: ES_SERVERS
# $2: Index name
function create_enrich_policies {
	echo -n $(date)" | Creating enrich-policy ... "
	response=$(curl  -u "$ES_USERNAME:$ES_PASSWORD" -s -X PUT "$1/_enrich/policy/match-paths-file-data-policy-$2?pretty" -H 'Content-Type: application/json' -d'
	{
		"match": {
			"indices": "'$2'-paths",
			"match_field": "file_tag",
			"enrich_fields": ["file_path", "file_type"]
		}
	}
	')
	# echo $response
	status=$(echo $response | jq '.status')
	error=$(echo $response | jq '.error')
	error_type=$(echo $response | jq '.error.type')
	error_reason=$(echo $response | jq '.error.reason')
	if [[ $error == "null" ]]; then
		echo $response | jq '.acknowledged';
		return 1;
	elif [[ $status -eq 400 && $error_type == "\"resource_already_exists_exception\"" ]]; then
		echo "resource already exists!"
		return 1;
	else
		echo "Error: $error_type: $error_reason";
		return 2;
	fi;
}

# $1: ES_SERVERS
# $2: Index name
function execute_enrich_policies {
	echo -n $(date)" | Executing enrich-policy ... "
	response=$(curl  -u "$ES_USERNAME:$ES_PASSWORD" -s -X PUT  "$1/_enrich/policy/match-paths-file-data-policy-$2/_execute?pretty")
	status=$(echo $response | jq '.status')
	error=$(echo $response | jq '.error')
	error_type=$(echo $response | jq '.error.type')
	error_reason=$(echo $response | jq '.error.reason')
	if [[ $error == "null" ]]; then
		echo $response | jq '.status.phase';
	else
		echo "Error: $error_type: $error_reason";
	fi;
}

# $1: ES_SERVERS
# $2: Index name
function create_ingest_pipeline {
	echo -n $(date)" | Creating ingest pipeline ... "
	response=$(curl  -u "$ES_USERNAME:$ES_PASSWORD" -s -X PUT "$1/_ingest/pipeline/match-files-data-pipeline-$2?pretty" -H 'Content-Type: application/json' -d'
	{
		"description": "Enrich file information",
		"processors" : [
			{
				"enrich": {
					"field": "file_tag",
					"policy_name": "match-paths-file-data-policy-'$2'",
					"target_field": "fdata",
					"if": "if ( ctx.fdata == null || ctx.fdata.file_path == null) return true;\nelse return false;",
					"on_failure": [
						{
							"set": {
								"field": "enrich_error",
								"value": "{{ _ingest.on_failure_message }}"
							}
						}
					]
				}
			},
			{
				"remove": {
					"field": "fdata.file_tag",
					"ignore_failure": true,
					"if": "if ( ctx.fdata != null) return true;\nelse return false;"
				}
			},
			{
				"script" : {
					"inline" : "if (ctx.nvisited == null) { ctx.nvisited = params.value } else { ctx.nvisited++ }",
					"params" : {
						"value" : 1
					}
				}
			}
		]

	}')
	status=$(echo $response | jq '.status')
	error=$(echo $response | jq '.error')
	error_type=$(echo $response | jq '.error.type')
	error_reason=$(echo $response | jq '.error.reason')
	if [[ $error == "null" ]]; then
		echo $response | jq '.acknowledged';
	else
		echo "Error: $error_type: $error_reason";
	fi;
}

# $1: ES_SERVERS
# $2: Index name
# $3: N tries
# return 0 if updated 0, 1 if updated > 0, 2 if failure
function update_by_query {
	echo -n $(date)" | Executing update_by_query ... "
	response=$(curl  -u "$ES_USERNAME:$ES_PASSWORD" -s -X POST "$1/$2/_update_by_query?refresh=true&scroll_size=5000&slices=auto&pipeline=match-files-data-pipeline-$2" -H 'Content-Type: application/json' -d'
	{
		"query": {
			"bool": {
			"must": { "exists": { "field": "file_tag" } },
			"must_not": { "exists": { "field": "fdata.file_path" } },
			"filter": [
				{"bool": {"must_not": {"range": {"nvisited": {"gte": '$3' } }}}}
			]
			}
		}
	}')

	# echo $response
	status=$(echo $response | jq '.status')
	error=$(echo $response | jq '.error')
	error_type=$(echo $response | jq '.error.type')
	error_reason=$(echo $response | jq '.error.reason')
	if [[ $error == "null" ]]; then
		took=$(echo $response | jq '.took');
		total=$(echo $response | jq '.total');
		updated=$(echo $response | jq '.updated');
		failures=$(echo $response | jq '.failures');
		echo " updated $updated of $total documents in $took ms"
		if [[ $failures != "[]" ]]; then
			echo "Failures: $failures";
			return 2;
		fi;
		if [ $updated -gt 0 ]; then return 1; else return 0; fi;
	else
		echo "Error: $error_type: $error_reason";
	fi;
}

# $1: ES_SERVERS
# $2: Index name
function delete_enrich_policy {
	response=$(curl  -u "$ES_USERNAME:$ES_PASSWORD" -s -X DELETE "$1/_enrich/policy/match-paths-file-data-policy-$2")
}

# $1: ES_SERVERS
# $2: Index name
function delete_ingest_pipeline {
	response=$(curl  -u "$ES_USERNAME:$ES_PASSWORD" -s -X DELETE "$1/_ingest/pipeline/match-files-data-pipeline-$2")
}

function updateIndex {
	CREAT_POLICY=0
	i=1
	WAIT_FOR_EVENTS=false

	NDOCS=0
	tries=3
	while [ $tries -gt 0 ]; do
		echo "Checking if index $INDEX-paths contains documents"
		response=$(curl -u "$ES_USERNAME:$ES_PASSWORD" -s -X GET "$ES_SERVERS/$INDEX-paths/_count")
		# echo "Response: $response"
		error=$(echo $response | jq '.error')
		error_type=$(echo $response | jq '.error.type')
		error_reason=$(echo $response | jq '.error.reason')
		if [[ $error == "null" ]]; then
			NDOCS=$(echo $response | jq '.count')
			if [[ $NDOCS -gt 0 ]]; then
				echo "Found $NDOCS documents on index $INDEX-paths"
				break;
			fi
		else
			echo "No documents found on index $INDEX-paths"
		fi
		tries=$((tries-1))
		echo "Waiting 30 seconds before retrying..."
		sleep 30
	done

	if [[ $NDOCS -eq 0 ]]; then
		echo "No documents found on index $INDEX. Skipping..."
		clean_exit 1
	fi

	echo $(date)" | Correlating data from index '$INDEX'"
	while [ $i -gt 0 ]; do
		if [[ "$WAIT_FOR_EVENTS" = true ]]; then
			echo $(date)" | waiting 30s before retry"
			sleep 30s
			WAIT_FOR_EVENTS=false
		fi
		echo "-------------------------------------"
		if [ $CREAT_POLICY -eq 0 ]; then
			create_enrich_policies $ES_SERVERS $INDEX
			CREAT_POLICY=$?
			if [ $CREAT_POLICY -eq 2 ]; then
				WAIT_FOR_EVENTS=true
				CREAT_POLICY=0
				continue;
			fi
			execute_enrich_policies $ES_SERVERS $INDEX
			create_ingest_pipeline $ES_SERVERS $INDEX
		else
			execute_enrich_policies $ES_SERVERS $INDEX
		fi
		update_by_query $ES_SERVERS $INDEX $N_TRIES
		i=$?
		if [[ $i -eq 2 && "$WAITING" = true ]] ; then break; fi;
		echo $(date)" | Sleeping "$SLEEP" seconds ...";
		sleep $SLEEP;
	done

	delete_enrich_policy $ES_SERVERS $INDEX
	delete_ingest_pipeline $ES_SERVERS $INDEX
}

# $1 = elasticsearch url
# $2 = time to sleep
# $3 = index name
function correlate_data {

	ES_SERVERS=$1
	SLEEP=$2
	N_TRIES=$3

	echo "ES_SERVERS: $ES_SERVERS, SLEEP: $SLEEP, N_TRIES: $N_TRIES, $4"

	curl -u "dio:diopw" -s -X GET "$ES_SERVERS" > /dev/null
	if [ $? -ne 0 ]; then
		echo "Error: Could not connect to $ES_SERVERS"
		exit 1
	fi

	echo "Starting inotifywait ..."

	WAITING=false
	WAIT_FOR_EVENTS=false

	TARGET_DIR=/tmp/dio
	inotifywait -m $TARGET_DIR -e create,delete -e moved_to |
    while read dir action file; do
        echo "The file '$file' appeared in directory '$dir' via '$action'"
		if [[ $action == "CREATE" ]]; then
			WAITING=false
			SESSION_NAME=`cat $TARGET_DIR/$file | awk '{print tolower($0)}'` ;
			INDEX="dio_trace_$SESSION_NAME"
			updateIndex
		elif [[ $action == "DELETE" ]]; then
			WAITING=true;
			if [[ "$4" = "true" ]]; then
				echo "Exiting ..."
				clean_exit 1
			fi
		fi
	done

}

function clean_exit {
  kill $(pgrep inotifywait)
}

correlate_data "${@:2}"
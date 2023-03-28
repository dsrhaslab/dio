import sys
import json
import random
import string
import logging
import requests
from datetime import datetime
from requests.auth import HTTPBasicAuth
from elasticsearch import Elasticsearch
from elasticsearch.client.ingest import IngestClient


URL_ELASTIC   = "http://localhost:31111/"
HEADERS       = {'Content-type': 'application/json', 'Accept': 'text/plain'}


BULK_SIZE     = 1000
BULK_INDEX_ID = 1
LAST_BULK_IDX = BULK_INDEX_ID
RECORDS_SAVED = []

ELASTIC_CLIENT = Elasticsearch(['http://localhost:31111'], http_auth=('dio', 'diopw'))

logging.getLogger('elasticsearch').setLevel(logging.WARNING)

#time map to calculate tracing duration
time = {"min_t": -1, "max_t": 0, "duration": 0}


def main():
	global logger
	logger = setup_logger()
	logger.info("running...")

	global SID
	SID = session_id_generator()
	global INDEX_NAME
	INDEX_NAME = "dio_trace_{}".format(SID)

	create_indexes(ELASTIC_CLIENT, INDEX_NAME)
	create_ingest_pipeline(ELASTIC_CLIENT)

	# Parse and export data
	parse_catlog(sys.argv[1])

	logger.warning("generated_session: {}".format(SID))

	logger.info("process exited...")

def setup_logger():
	logger = logging.getLogger('')
	logger.setLevel(logging.DEBUG)
	sh = logging.StreamHandler(sys.stdout)
	formatter = logging.Formatter('[%(asctime)s] %(levelname)s %(message)s', datefmt='%a, %d %b %Y %H:%M:%S')
	sh.setFormatter(formatter)
	logger.addHandler(sh)
	return logger

def parse_catlog(filename):

	RECORDS_SAVED = []
	BULK_INDEX_ID = 0
	LAST_BULK_IDX = 0
	TIME_MIN_STR = ""
	TIME_MAX_STR = ""

	try:
		with open(filename, 'r') as file:

			# read file line by line
			for line in file:

				# flush records if size equals BULK_SIZE
				if len(RECORDS_SAVED) == BULK_SIZE:
					bulk_json_list(RECORDS_SAVED, LAST_BULK_IDX)
					logger.info("bulking {} records ({} to {})...".format(BULK_SIZE, LAST_BULK_IDX, BULK_INDEX_ID))
					LAST_BULK_IDX = BULK_INDEX_ID
					RECORDS_SAVED = []

				# parse line to a json object
				jsonObject = line2json(line)
				jsonObject["session_name"] = SID
				if ("call_timestamp" in jsonObject) and (time["min_t"] == -1 or jsonObject["call_timestamp"] < time["min_t"]):
					time["min_t"] = jsonObject["call_timestamp"]
					TIME_MIN_STR = jsonObject["time_called"]
				if ("return_timestamp" in jsonObject) and (jsonObject["return_timestamp"] > time["max_t"]):
					time["max_t"] = jsonObject["return_timestamp"]
					TIME_MAX_STR = jsonObject["time_called"]

				# add json object to list of records
				RECORDS_SAVED.append(jsonObject)
				BULK_INDEX_ID = BULK_INDEX_ID + 1

			# verify if there is any record to flush
			if len(RECORDS_SAVED) > 0:
				bulk_json_list(RECORDS_SAVED, LAST_BULK_IDX)
				logger.info("bulking {} records ({} to {})...".format(BULK_SIZE, LAST_BULK_IDX, BULK_INDEX_ID))
				LAST_BULK_IDX = BULK_INDEX_ID
				RECORDS_SAVED = []

		# Creating json of duration
		time["duration"] = time["max_t"] - time["min_t"]
		time["session_name"] = SID
		time["min_t"] = TIME_MIN_STR
		time["max_t"] = TIME_MAX_STR
		print(time)

		export_record(INDEX_NAME, time)

	except requests.exceptions.RequestException as e:
		logger.error(e)
	except IOError:
		logger.error("could not load the provided file")

	logger.info("all data was exported...")


def session_id_generator(info=""):
	random_str = ''.join(random.choice(string.ascii_lowercase) for _ in range(6))
	time_now   = datetime.now().strftime("%d.%m.%Y-%H.%M.%S")

	sid = "{}_{}".format(random_str, time_now)

	if not (info is None) and len(info) > 0:
		sid = sid + "_info=" + info.lower().replace(" ", "_")

	return sid

def bulk_json_list(records, begin_idx):
	bulkArr = []
	for json in records:
		bulkArr.append({'index': {'_id': begin_idx}})
		bulkArr.append(json)
		begin_idx = begin_idx + 1
	ELASTIC_CLIENT.bulk(index = INDEX_NAME, body=bulkArr, pipeline='split-events-pipeline')

def export_record(sid, record):

	jsonObj = json.dumps(record, indent=4)
	url = "{}{}/_doc".format(URL_ELASTIC, sid)
	x = requests.post(url, data=jsonObj, headers = HEADERS, auth = HTTPBasicAuth('dio', 'diopw'))

	print(x.text)
	return x.status_code


def line2json(line):
	return json.loads(line)


def create_indexes(es_client, index_name):
	mapping = '''
	{
		"mappings": {
			"properties": {
				"time_called": { "type": "date_nanos" },
				"time_returned": { "type": "date_nanos" }
			}
		}
	}'''

	es_client.indices.create(index=index_name, body=mapping)
	es_client.indices.create(index=index_name+"-paths")

def create_ingest_pipeline(es_client):
	p = IngestClient(es_client)
	p.put_pipeline(id='split-events-pipeline', body={
		"description": "Split system calls into different indexes",
		"processors": [
			{
				"set": {
					"field": "_index",
					"value": "{{{ _index }}}-paths",
					"if": "if ( ctx.doc_type == \"EventPath\") { return true; }",
					"ignore_failure": True
				}
			}
		]
	})

if __name__ == '__main__':
	main()
import argparse
import traceback
import json
import logging
import time
from sys import exit
from elasticsearch import Elasticsearch
from elasticsearch.client import IngestClient


logger = logging.getLogger("DoParser")
SENT_EVENTS = 0
SENT_BULKS = 0

def prepare_indices(es_conn, session):
	index = "dio_trace_{}".format(session)

	# Create New DIO Tracer Index
	mappings={
		"properties": {
			"time_called": { "type": "date_nanos" },
			"time_returned": { "type": "date_nanos" }
		}
	}
	es_conn.indices.create(index=index, mappings=mappings, ignore=400)


	# Create New Index for Paths
	es_conn.indices.create(index=index+"-paths", ignore=400)


	# Create DIO Ingest Pipeline
	p = IngestClient(es_conn)
	pipeline = {
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
	}
	p.put_pipeline(id='split-events-pipeline', body=pipeline)

	return index

def bulkIndex(es_conn, records, begin_idx, index, session=None, pipeline=None):
        # Index a bulk of documents
        bulkArr = []
        for json in records:
            if session is not None:
                id = "{}_{}".format(session, begin_idx)
            else:
                id = begin_idx
            bulkArr.append({'index': {'_id': "{}".format(id)}})
            bulkArr.append(json)
            begin_idx = begin_idx + 1
        res = es_conn.bulk(index = index, body=bulkArr, pipeline=pipeline)
        # print(res)
        errors = {}
        for val in res["items"]:
            if "error" in val["index"]:
                if val["index"]["error"]["reason"] not in errors:
                    errors[val["index"]["error"]["reason"]] = 1
                else:
                    errors[val["index"]["error"]["reason"]] += 1
        return errors, res["took"]


def bulk_data(es_conn, bulk, bulk_start_index, bulk_end_index, index, session):
	errors, took = bulkIndex(es_conn, bulk, bulk_start_index, index, session, "split-events-pipeline")
	if errors:
		logger.error("Got following errors while bulking: ")
		for error in errors:
			logger.error("\t- {} (x{})".format(error, errors[error]))
		_finish()
	else:
		logger.debug("bulked {} records ({} to {}) in {} ms".format(len(bulk), bulk_start_index, bulk_end_index, took))
	global SENT_EVENTS
	SENT_EVENTS += len(bulk)
	global SENT_BULKS
	SENT_BULKS += 1
	bulk_start_index = bulk_end_index
	bulk = []
	return bulk, bulk_start_index, bulk_end_index


def parse_tracer(es_conn, session, filename, bulk_size=1000):
	logger.info("Parsing file: {}".format(filename))

	# parse file and save records to ES
	bulk = []
	bulk_start_index = 0
	bulk_end_index = 0
	min_time_str = ""
	max_time_str = ""
	time = {"min_t": -1, "max_t": 0, "duration": 0}
	index = None
	sent_events = 0

	if session is not None:
		newSessionName = True
		logger.info("Session name: {}".format(session))
	else:
		newSessionName = False

	try:
		with open(filename, 'r') as file:

			# read file line by line
			for line in file:
				# parse line to a json object
				if line.endswith('\n'):
					line = line[:-1]
				if line.endswith(',') or line.endswith(']'):
					line = line[:-1] + "]"

				if line.startswith('{'):
					line = "[" + line

				jsonObject = json.loads(line)

				for obj in jsonObject:

					# flush records if size equals bulk_size
					if len(bulk) == bulk_size:
						if index is None:
							index = prepare_indices(es_conn, session)
						bulk, bulk_start_index, bulk_end_index = bulk_data(es_conn, bulk, bulk_start_index, bulk_end_index, index, session)

					if newSessionName:
						obj["session_name"] = session
					elif session is None:
						session = obj["session_name"]
						logger.info("Session name: {}".format(session))

					if ("call_timestamp" in obj) and (time["min_t"] == -1 or obj["call_timestamp"] < time["min_t"]):
						time["min_t"] = obj["call_timestamp"]
						min_time_str = obj["time_called"]

					if ("return_timestamp" in obj) and (obj["return_timestamp"] > time["max_t"]):
						time["max_t"] = obj["return_timestamp"]
						max_time_str = obj["time_called"]

					# add json object to list of records
					bulk.append(obj)
					bulk_end_index += 1

			# verify if there is any record to flush
			if len(bulk) > 0:
				if index is None:
					index = prepare_indices(es_conn, session)
				bulk, bulk_start_index, bulk_end_index = bulk_data(es_conn, bulk, bulk_start_index, bulk_end_index, index, session)

		logger.info("Sent %d records to ES" % bulk_end_index)

		# Creating doc of duration
		time["duration"] = time["max_t"] - time["min_t"]
		time["session_name"] = session
		time["min_t"] = min_time_str
		time["max_t"] = max_time_str
		es_conn.index(index=index, document=time, id="{}_{}".format(session, bulk_end_index+1), request_timeout=300)
		logger.info("Sent duration doc to ES: {}".format(time))

	except IOError:
		logger.error("could not load the provided file")


def _start():
	logger.setLevel(logging.INFO)

	# create console handler and set level to debug
	ch = logging.StreamHandler()
	ch.setLevel(logging.INFO)

	# create formatter
	# formatter = logging.Formatter('%(asctime)s [ %(name)s | %(levelname)s ] %(message)s')
	formatter = logging.Formatter('[%(name)s][%(asctime)s] %(levelname)s %(message)s', datefmt='%a, %d %b %Y %H:%M:%S')

	# add formatter to ch
	ch.setFormatter(formatter)

	# add ch to logger
	logger.addHandler(ch)

	logger.info("DoParser Started!")


def _finish():
	logger.info("DoParser Finished!")
	exit(0)


def main():

	start_time = time.time()

	parser = argparse.ArgumentParser(description='Parses DIO trace file and export to ElasticSearch')
	parser.add_argument('-u', '--url',  default="http://cloud124:31111", type=str, help='elasticSearch URL')
	parser.add_argument('--session', help='session name', default=None, nargs='?')
	parser.add_argument('--size', metavar='size', default=1000, type=int, help='bulk size')
	parser.add_argument('-d', '--debug', action='store_true', default=False, help='Debug mode')
	parser.add_argument('file', help='DIO trace file', default=None, nargs='?')

	args = parser.parse_args()

	_start

	try:

		if args.debug:
			logger.setLevel(logging.DEBUG)

		es_conn = Elasticsearch([args.url], basic_auth=('dio', 'diopw'))

		if args.file == None:
			logger.error("A valid file must be provided.")
			_finish()

		parse_tracer(es_conn, args.session, args.file, args.size)

	except Exception as e:

		logger.error("Got an unexpected error: %s" % e)
		traceback.print_exception(type(e), e, e.__traceback__)

	print("--- Took %s seconds. Indexed %d events with %d bulks ---" % ((time.time() - start_time), SENT_EVENTS, SENT_BULKS))
	_finish()


if __name__ == '__main__':
	main()
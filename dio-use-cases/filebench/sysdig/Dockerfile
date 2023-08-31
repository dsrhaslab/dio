FROM docker.elastic.co/logstash/logstash:8.5.2 as logstash_image

FROM sysdig/sysdig:0.31.4

COPY --from=logstash_image /usr/share/logstash /usr/share/logstash

RUN yum install -y time jq curl bc

ENV ES_HOSTS="http://localhost:9200"
ENV ES_USERNAME="elastic"
ENV ES_PASSWORD="elastic"
ENV SYSDIG_COMMAND="sysdig -t a"
ENV LS_BATCH_SIZE=125
ENV LS_BATCH_DELAY=50

COPY config/logstash.yml /usr/share/logstash/config/logstash.yml
COPY config/sysdig-logstash.conf /usr/share/logstash/sysdig-logstash.conf

COPY scripts/entrypoint.sh /usr/share/sysdig/entrypoint.sh
RUN chmod +x /usr/share/sysdig/entrypoint.sh

COPY scripts/entrypoint-file.sh /usr/share/sysdig/entrypoint-file.sh
RUN chmod +x /usr/share/sysdig/entrypoint-file.sh

COPY scripts/entrypoint-dev-null.sh /usr/share/sysdig/entrypoint-dev-null.sh
RUN chmod +x /usr/share/sysdig/entrypoint-dev-null.sh

COPY scripts/get_events_stats.sh /usr/share/sysdig/get_events_stats.sh
RUN chmod +x /usr/share/sysdig/get_events_stats.sh

COPY scripts/get_events_stats_from_es.sh /usr/share/sysdig/get_events_stats_from_es.sh
RUN chmod +x /usr/share/sysdig/get_events_stats_from_es.sh

CMD [ "/usr/share/sysdig/entrypoint.sh" ]


FROM taniaesteves/dio-tracer:v1.0.0

RUN apt-get update && apt-get install -y openjdk-17-jdk

# Create a user 'elasticsearch' under 'elasticsearch'
RUN useradd -d /usr/share/elasticsearch -M -s /bin/bash -U elasticsearch

COPY elasticsearch-8.3.0-SNAPSHOT-linux-x86_64.tar.gz .

RUN tar -xzvf elasticsearch-8.3.0-SNAPSHOT-linux-x86_64.tar.gz -C /usr/share/ && \
    rm elasticsearch-8.3.0-SNAPSHOT-linux-x86_64.tar.gz

RUN chown -R elasticsearch:elasticsearch /usr/share/elasticsearch-8.3.0-SNAPSHOT

WORKDIR /usr/share/elasticsearch-8.3.0-SNAPSHOT

RUN echo "xpack.security.enabled: false" >> /usr/share/elasticsearch-8.3.0-SNAPSHOT/config/elasticsearch.yml && \
    echo "discovery.type: single-node" >> /usr/share/elasticsearch-8.3.0-SNAPSHOT/config/elasticsearch.yml

RUN sed -i 's/#network.host: 192.168.0.1/network.host: 0.0.0.0/g' /usr/share/elasticsearch-8.3.0-SNAPSHOT/config/elasticsearch.yml

RUN apt-get install -y strace time

COPY config/dio-es.yaml /usr/share/dio/conf/config.yaml

COPY scripts/start_es.sh ./start_es.sh
RUN chmod +x start_es.sh

ENTRYPOINT [ "/usr/share/elasticsearch-8.3.0-SNAPSHOT/start_es.sh" ]
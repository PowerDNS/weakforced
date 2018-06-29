FROM docker.elastic.co/elasticsearch/elasticsearch:6.2.0
RUN bin/elasticsearch-plugin remove x-pack
RUN sed -i '/xpack/d' /usr/share/elasticsearch/config/elasticsearch.yml

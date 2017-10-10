FROM docker.elastic.co/logstash/logstash:5.6.2

USER root
RUN yum install -y wget
RUN wget http://geolite.maxmind.com/download/geoip/database/GeoLite2-City.mmdb.gz
RUN gunzip GeoLite2-City.mmdb.gz
RUN mv GeoLite2-City.mmdb /usr/share/logstash/GeoLiteCity.dat
USER logstash

RUN logstash-plugin install logstash-input-udp
RUN logstash-plugin install logstash-output-elasticsearch
RUN logstash-plugin install logstash-filter-geoip
RUN logstash-plugin remove x-pack
RUN sed -i '/xpack/d' /usr/share/logstash/config/logstash.yml

FROM docker.elastic.co/logstash/logstash:6.2.0

USER root
RUN yum install -y wget
RUN mkdir -p /usr/share/logstash/geoip
RUN chown -R logstash:logstash /usr/share/logstash/geoip
RUN wget -O GeoLite2-City.mmdb.tar.gz "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-City&license_key=SPdi9BhAhMj6oOpw&suffix=tar.gz"
RUN gunzip GeoLite2-City.mmdb.tar.gz
RUN tar xvf GeoLite2-City.mmdb.tar
RUN mv GeoLite2*/GeoLite2-City.mmdb /usr/share/logstash/geoip
USER logstash

RUN logstash-plugin install logstash-input-udp
RUN logstash-plugin install logstash-output-elasticsearch
RUN logstash-plugin install logstash-filter-geoip
RUN logstash-plugin remove x-pack
RUN sed -i '/xpack/d' /usr/share/logstash/config/logstash.yml

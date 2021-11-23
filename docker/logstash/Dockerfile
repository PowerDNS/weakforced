FROM docker.elastic.co/logstash/logstash:7.14.2

USER root
RUN yum install -y wget
RUN mkdir -p /usr/share/logstash/geoip
RUN chown -R logstash:logstash /usr/share/logstash/geoip
ARG MAXMIND_LICENSE_KEY
# Don't fail if license key isn't set
COPY geoip/GeoIP2-City-Test.mmdb /usr/share/logstash/geoip/GeoLite2-City.mmdb
RUN wget -O GeoLite2-City.mmdb.tar.gz "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-City&license_key=${MAXMIND_LICENSE_KEY}&suffix=tar.gz" || true
RUN gunzip GeoLite2-City.mmdb.tar.gz || true
RUN tar xvf GeoLite2-City.mmdb.tar || true
RUN mv GeoLite2*/GeoLite2-City.mmdb /usr/share/logstash/geoip || true
USER logstash

RUN logstash-plugin install logstash-input-udp
RUN logstash-plugin install logstash-output-elasticsearch
RUN logstash-plugin install logstash-filter-geoip

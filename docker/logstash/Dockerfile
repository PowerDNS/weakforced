FROM logstash:5

RUN wget http://geolite.maxmind.com/download/geoip/database/GeoLite2-City.mmdb.gz
RUN gunzip GeoLite2-City.mmdb.gz
RUN mv GeoLite2-City.mmdb /etc/logstash/GeoLiteCity.dat

RUN logstash-plugin install logstash-input-udp
RUN logstash-plugin install logstash-output-elasticsearch
RUN logstash-plugin install logstash-filter-geoip

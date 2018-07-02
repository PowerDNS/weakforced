DCMP = docker-compose
COMPOSE_SOURCE = docker-compose.yml elasticsearch/Dockerfile logstash/Dockerfile logstash/config/logstash.conf 
COMPOSE_TARGET = .docker
GEOIP_FILENAME = GeoLite2-City.mmdb
GEOIP_FILE_GZ = logstash/geoip/$(GEOIP_FILENAME).gz
GEOIP_FILE = logstash/geoip/$(GEOIP_FILENAME)

$(GEOIP_FILE_GZ):
	wget -N -O $(GEOIP_FILE_GZ) http://geolite.maxmind.com/download/geoip/database/$(GEOIP_FILENAME).gz

$(GEOIP_FILE): $(GEOIP_FILE_GZ)
	gunzip -c $(GEOIP_FILE_GZ) >$(GEOIP_FILE)

$(COMPOSE_TARGET): $(COMPOSE_SOURCE) $(GEOIP_FILE)
	$(DCMP) down -v
	$(DCMP) build
	touch $(COMPOSE_TARGET)

build_image: $(COMPOSE_TARGET)

start:
	$(DCMP) up -d

stop:
	$(DCMP) stop

kill:
	$(DCMP) kill

clean_elastic: stop
	$(DCMP) rm -v
	docker volume rm docker_esdata
	rm $(COMPOSE_TARGET)

clean_geoip:
	rm -rf logstash/geoip/*

clean_docker:
	docker-compose down -v

clean: clean_docker clean_geoip

all: build_image start

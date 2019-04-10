DCMP = docker-compose
COMPOSE_SOURCE = docker-compose.yml elasticsearch/Dockerfile logstash/Dockerfile logstash/config/logstash.conf logstash/templates/wforce_template.json regression/Dockerfile
COMPOSE_TARGET = .docker
GEOIP_FILENAME = GeoLite2-City.mmdb
GEOIP_FILE_GZ = logstash/geoip/$(GEOIP_FILENAME).gz
GEOIP_FILE = logstash/geoip/$(GEOIP_FILENAME)
REGRESSION_SERVICE = regression

$(GEOIP_FILE_GZ):
	wget -N -O $(GEOIP_FILE_GZ) http://geolite.maxmind.com/download/geoip/database/$(GEOIP_FILENAME).gz

$(GEOIP_FILE): $(GEOIP_FILE_GZ)
	gunzip -c $(GEOIP_FILE_GZ) >$(GEOIP_FILE)

$(COMPOSE_TARGET): $(COMPOSE_SOURCE) $(GEOIP_FILE) $(shell find ../common -type f) $(shell find ../wforce -type f) $(shell find ../trackalert -type f) $(shell find ../ext -type f) 
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

regression-gcc: build_image start
	$(DCMP) exec $(REGRESSION_SERVICE) docker/regression/regression.sh gcc g++

regression-clang: build_image start
	$(DCMP) exec $(REGRESSION_SERVICE) docker/regression/regression.sh clang-4.0 clang++-4.0

regression-none:
	echo "Regression tests skipped due to compiler 'none'"

regression: regression-gcc regression-clang

all: build_image start

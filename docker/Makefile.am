DCMP = docker-compose
COMPOSE_SOURCE = docker-compose.yml elasticsearch/Dockerfile logstash/Dockerfile logstash/config/logstash.conf logstash/templates/wforce_template.json regression/Dockerfile wforce_image/Dockerfile wforce_image/docker-entrypoint.sh wforce_image/wforce.conf.j2 wforce_image/create_config.sh
COMPOSE_TARGET = .docker
REGRESSION_SERVICE = regression
export BUILD_DATE= $(shell date)
export LICENSE= $(shell echo GPLv3)
export GIT_REVISION= $(shell git rev-parse --short HEAD)
export VERSION = $(shell git describe --tags)

$(COMPOSE_TARGET): $(COMPOSE_SOURCE) $(shell find ../common -type f) $(shell find ../wforce -type f) $(shell find ../trackalert -type f) $(shell find ../ext -type f) $(shell find ../regression-tests -type f) $(shell find wforce_image -type f) $(shell find regression -type f)  $(shell find logstash -type f)  $(shell find elasticsearch -type f)
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
	$(DCMP) exec $(REGRESSION_SERVICE) docker/regression/regression.sh clang clang++

regression-none:
	echo "Regression tests skipped due to compiler 'none'"

regression: regression-gcc regression-clang

all: build_image start

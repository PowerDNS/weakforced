DCMP = docker-compose
WFORCE_IMAGE_COMPOSE_TARGET = .wforce_image_docker
WFORCE_IMAGE_SERVICE = wforce_image
COMPOSE_SOURCE=Dockerfile create_config.sh docker-compose.yml docker-entrypoint.sh wforce.conf.j2
export BUILD_DATE= $(shell date)
export LICENSE= $(shell echo GPLv3)
export GIT_REVISION= $(shell git rev-parse --short HEAD)
export VERSION = $(shell git describe --tags)

$(WFORCE_IMAGE_COMPOSE_TARGET): $(shell find weakforced/common -type f) $(shell find weakforced/wforce -type f) $(shell find weakforced/trackalert -type f) $(shell find weakforced/ext -type f) $(shell find weakforced/report_api -type f) $(COMPOSE_SOURCE)
	$(DCMP) down -v
	$(DCMP) build
	touch $(WFORCE_IMAGE_COMPOSE_TARGET)

start:
	$(DCMP) up -d

stop:
	$(DCMP) stop

kill:
	$(DCMP) kill

clean_docker:
	docker-compose down -v

clean: clean_docker

build_wforce_image: $(WFORCE_IMAGE_COMPOSE_TARGET)

test_wforce_image: build_wforce_image
	$(DCMP) up -d
	$(DCMP) exec $(WFORCE_IMAGE_SERVICE) test -f /usr/bin/wforce
	$(DCMP) exec $(WFORCE_IMAGE_SERVICE) test -f /usr/bin/trackalert
	$(DCMP) exec $(WFORCE_IMAGE_SERVICE) test -f /etc/wforce/wforce.conf
	$(DCMP) exec $(WFORCE_IMAGE_SERVICE) test -f /etc/wforce/trackalert.conf
	$(DCMP) exec $(WFORCE_IMAGE_SERVICE) wforce -e 'showStringStatsDB()'
	$(DCMP) stop



ACLOCAL_AMFLAGS = -I m4

if WITH_DOCKER
BUILD_DOCKER = docker
endif
if WITH_TRACKALERT
BUILD_TRACKALERT = trackalert
endif

SUBDIRS=ext docs common wforce $(BUILD_TRACKALERT) $(BUILD_DOCKER)

LOGSTASH_DIST = elk/logstash/config/logstash.conf elk/logstash/templates/wforce_template.json elk/kibana/kibana_saved_objects.ndjson

EXTRA_DIST= README.md LICENSE CHANGELOG.md $(LOGSTASH_DIST)

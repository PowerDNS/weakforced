#!/bin/bash
TAG=`git describe --tags`
echo "$DOCKER_PASSWORD" | docker login -u "$DOCKER_USERNAME" --password-stdin
docker push powerdns/wforce:$TAG

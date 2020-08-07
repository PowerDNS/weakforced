#!/bin/bash
TAG=`git describe --tags`
echo "Docker username is: '"$DOCKER_USERNAME"'"
echo "$DOCKER_PASSWORD" | docker login --username "$DOCKER_USERNAME" --password-stdin
docker push powerdns/wforce:$TAG

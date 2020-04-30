#!/bin/bash
TAG=`git describe --tags`
echo "$DOCKER_PASSWORD" | docker login -u "$DOCKER_USERNAME" --password-stdin
docker tag powerdns/wforce:$TAG neilcookster/wforce:$TAG
docker push neilcookster/wforce:$TAG
# Enable this at some point
# docker push powerdns/wforce:$TAG

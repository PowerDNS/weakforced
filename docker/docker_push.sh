#!/bin/bash
set -e
docker_login()
{
    echo "$DOCKER_PASSWORD" | docker login --username "$DOCKER_USERNAME" --password-stdin
}
# Only push tagged releases matching the versioning scheme
check_version()
{
    echo $TAG | egrep "^v[0-9]+\.[0-9]+\.[0-9]+$"
}
TAG=`git describe --tags`
check_version
if [ $? = "0" ]
then
    echo "Docker username is: '"$DOCKER_USERNAME"'"
    docker_login
    docker push powerdns/wforce:$TAG
fi

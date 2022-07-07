#!/bin/bash
docker_login()
{
    echo "$DOCKER_PASSWORD" | docker login --username "$DOCKER_USERNAME" --password-stdin
}
# Only push tagged releases matching the versioning scheme
check_version()
{
    echo $TAG | egrep "^v[0-9]+\.[0-9]+\.[0-9]+$"
}

push_tag()
{
  $tag = $1
  echo "Docker username is: '"$DOCKER_USERNAME"'"
  docker_login
  docker push powerdns/wforce:$tag
}

TAG=`git describe --tags`
check_version
if [ $? = "0" ]
then
    push_tag $TAG
fi

BRANCH=`git branch --show-current`
if [ "$BRANCH" = "master" ]
then
    docker tag powerdns/wforce:$TAG powerdns/wforce:unstable
    push_tag unstable
fi

exit 0

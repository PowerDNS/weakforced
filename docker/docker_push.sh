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
  local tag=$1
  echo "Docker username is: '"$DOCKER_USERNAME"'"
  docker_login
  docker push $IMAGE:$tag
}

IMAGE=$1
TAG=`git describe --tags`
check_version
if [ $? = "0" ]
then
    push_tag $TAG
fi


BRANCH=`git branch --show-current`
if [ "$BRANCH" = "master" ]
then
    docker tag $IMAGE:$TAG $IMAGE:unstable
    push_tag unstable
fi

exit 0

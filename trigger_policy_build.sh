#!/bin/bash
# Build the weakforced-policy repo
export GIT_COMMIT=`git show --oneline -s`

body="{
 \"request\": {
 \"message\": \"Auto-triggered build from: $GIT_COMMIT\",
 \"branch\":\"master\"
}}"

curl -s -X POST \
 -H "Content-Type: application/json" \
 -H "Accept: application/json" \
 -H "Travis-API-Version: 3" \
 -H "Authorization: token $AUTHTOKEN" \
 -d "$body" \
 https://api.travis-ci.com/repo/PowerDNS%2Fweakforced-policy/requests

exit 0

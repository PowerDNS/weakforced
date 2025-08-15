# Release Notes for OX Abuse Shield 3.0.0

<!-- {% raw %} -->

## Improvements 
* Support new `fail_type` parameter for determining why a login failed
* New livez and readyz endpoints (unauthenticated) for k8s environments
* Support JA3 allow/block lists in API/Lua
* Allow ja3 to be passed to the reset API command
* Add support for building amazon-2023 packages
* Use asciidoctor to build documentation not pandoc
* Add a sample grafana dashboard for monitoring wforce
* Implemented the (existing but previously always 0) `wforce_active_http_connections` metric for counting active HTTP connections

## Removed
* Removed support for Enterprise Linux 7 and Amazon 2

<!-- {% endraw %} -->
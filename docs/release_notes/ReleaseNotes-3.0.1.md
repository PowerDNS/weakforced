# Release Notes for OX Abuse Shield 3.0.1

<!-- {% raw %} -->

## Bug Fixes
* Fix bug causing persistent blocklists to not load fully (stops at 1000 entries)

## Improvements 
* Don't log when recv replication queue is full, just increment a metric
* Add new metrics to track send and recv queue errors and size
* Setup new prometheus metrics for send queue size and set them instead of logging when queue size too big

<!-- {% endraw %} -->
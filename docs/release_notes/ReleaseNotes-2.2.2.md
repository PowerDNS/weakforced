# Release Notes for OX Abuse Shield 2.2.2

Bug Fixes/Changes
-----------------
* Fix control socket leak when client closes connection immediately

New Features
----------
* Support building debian buster packages
* StatsDBs can now be sharded for better performance (man wforce.conf
  for more info)
* StatsDB expiry thread now runs more often by default
* StatsDB expiry thread sleep time is now configurable (man
  wforce.conf for more info)
* Support for Kafka REST Proxy in webhooks (man wforce_webhook for
  more info)

# Release Notes for OX Abuse Shield 2.10.1

<!-- {% raw %} -->

## Bug Fixes

* Fixed bug in GeoIP2 lookups where return values were not populated

## Fixed bug in GeoIP2 lookups where return values were not populated

The GeoIP2 LookupCity Lua function was never correctly implemented, so results were not exposed to Lua correctly.
This fix exposes the results using the correct method to ensure future operation.

<!-- {% endraw %} -->
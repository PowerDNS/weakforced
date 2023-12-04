# Release Notes for OX Abuse Shield 2.10.0

<!-- {% raw %} -->

## New Features

* Add Enterprise Linux 9 Build Target
* Option to use OpenSSL instead of Libsodium for encryption

## Removed Features
- Remove Legacy GeoIP from Packages and Dockerfiles/Images
- Remove the Report API from weakforced entirely

## Add Enterprise Linux 9 Build Target

Enterprise Linux 9-based systems are now supported as a build target. Oracle Linux 9 is used as the 
build environment, but the package should work on any EL-9 environment. Additionally, el-7, el-8 and el-9 aliases
are available as build targets.

## Option to use OpenSSL instead of Libsodium for encryption

When libsodium is not available, weakforced will now use openssl crypto functions instead for encryption, including
encryption between the client and the server, and replication encryption. OpenSSL encryption is used for the
docker image, but the default for built packages is still libsodium.

## Remove Legacy GeoIP from Packages and Dockerfiles/Images

The legacy GeoIP Library is no longer included in the packages or Dockerfiles/images for weakforced.

## Remove the report_api from weakforced entirely

The Report API has been removed from weakforced. This feature was never used (to my knowledge), and was creating
a significant burden in terms of the maintenance of the python dependencies.

<!-- {% endraw %} -->
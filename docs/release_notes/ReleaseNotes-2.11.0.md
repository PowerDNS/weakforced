# Release Notes for OX Abuse Shield 2.11.0

<!-- {% raw %} -->

## Improvements 
* Now builds a separate luajit package (`wforce-lua-dist`), based on the openresty luajit fork. This is to address some issues found with stock luajit. The package also includes some lua modules that wforce typically makes use of.
* Build the wforce-minimal image for both arm64 and amd64, and add provenance.
* Fix centos-7/el-7 builds to still work after centos-7 went EOL
* Add support for debian-bookworm, remove support for debian-buster

<!-- {% endraw %} -->
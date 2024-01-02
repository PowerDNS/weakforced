# Release Notes for OX Abuse Shield 2.4.1

## New Features

* Dynamic management of siblings via Lua functions and REST API
* Optional per-sibling encryption keys
* Packaging for Amazon Linux in pdns-builder

## Bug Fixes/Changes

* Fix issue where replication length bytes can be truncated causing syncDB problems

## Dynamic Management of Siblings via Lua functions

Before this release, siblings could only be defined as part of the startup configuration; 
there was no way to add or remove siblings dynamically while wforce was running. With this
release all sibling management functions in Lua can be used from the console to add/remove
siblings at runtime. In addition, per-sibling encryption keys can optionally be specified.

The complete set of sibling management functions is as follows:
* setSiblings()
* setSiblingsWithKey() (New)
* addSibling()
* addSiblingWithKey() (New)
* removeSibling() (New)

For full details, see the wforce.conf man page.

## Dynamic Management of Siblings via REST API

New REST API endpoints enable siblings to be managed dynamically.

The new REST API endpoints are as follows:
* /?command=addSibling
* /?command=removeSibling
* /?command=setSibling

For more details see the wforce OpenAPI specification, which is available at https://powerdns.github.io/weakforced/

Note that the REST API does not currently support TLS natively, so use of a HTTPS reverse proxy on localhost
is strongly recommended when specifying per-sibling encryption keys.

# Optional Per-Sibling Encryption Keys

All the methods of managing siblings (Lua or REST API) enable per-sibling encryption keys to be set.
Encryption keys are 32-byte strings that are Base-64 encoded before passing to the sibling
management functions or REST API.

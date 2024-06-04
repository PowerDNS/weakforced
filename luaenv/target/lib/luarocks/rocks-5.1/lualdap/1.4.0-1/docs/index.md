
## Overview

LuaLDAP is a simple interface from Lua to an LDAP client, in fact
it is a bind to [OpenLDAP](https://www.openldap.org) client
or [ADSI](https://docs.microsoft.com/en-us/windows/win32/adsi/about-adsi).

It enables a Lua program to:

* Connect to an LDAP server;
* Execute any operation (search, add, compare, delete, modify and rename);
* Retrieve entries and references of the search result.

## Status

It's developed for Lua 5.1, 5.2, 5.3 & 5.4, and OpenLDAP 2.3 or newer.

## Download

The sources are hosted on [Github](https://github.com/lualdap/lualdap).

The Teal type definition of this library is available
[here](https://github.com/teal-language/teal-types/blob/master/types/lualdap/lualdap.d.tl).

## Installation

LuaLDAP is available via [LuaRocks](https://luarocks.org/modules/fperrad/lualdap):

```sh
luarocks install lualdap
```

Debian packages are available on [Debian](https://packages.debian.org/sid/lua-ldap)
or [Ubuntu](https://packages.ubuntu.com/jammy/lua-ldap).

RPM packages are available on [Fedora](https://src.fedoraproject.org/rpms/lua-ldap).

More downstreams on [repology.org](https://repology.org/projects/?search=lualdap&maintainer=&category=&inrepo=&notinrepo=&repos=&families=&repos_newest=&families_newest=).

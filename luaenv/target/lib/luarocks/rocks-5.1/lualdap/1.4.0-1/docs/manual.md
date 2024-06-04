
# Introduction

LuaLDAP is a simple interface from Lua to an LDAP client, in fact
it is a bind to [OpenLDAP](https://www.openldap.org) client
or [ADSI](https://docs.microsoft.com/en-us/windows/win32/adsi/about-adsi).

LuaLDAP returns a single table (with Lua 5.1,
this table is also stored in the global variable named `lualdap`)
This table holds the functions used to instantiate an LDAP connection object.

A connection object offers methods to perform any operation on
the directory such as comparing values, adding new entries,
modifying attributes on existing entries, removing entries,
and the most common of all: searching.
Entries are represented as Lua tables; attributes are its fields.
The attribute values can be strings or tables of strings
(used to represent multiple values).

# Representing attributes

Many LDAP operations manage sets of attributes and values.
LuaLDAP provides a uniform way of representing them by using Lua tables.
The table attributes can be Lua string, a binary string (a string of bits),
or table of _n_ values indexed from 1 to _n_.
Some operations have different approaches that will be explained as necessary.

Here is a simple example:

```lua
entry = {
    an_attribute = "a value",
    other_attribute = {
        "first value of other attribute",
        "another value of other attribute",
    },
}
```

Attribute names cannot contain the `'\0'` character.

# Distinguished names

The distinguished name (DN) is the term used to identify an entry
on the directory information tree.
It is formed by the relative distinguished name (RDN) of the entry
and the distinguished name of its parent.
LuaLDAP will always use a string to represent the DN of any entry.

A more precise definition can be found on the LDAP documentation.
A list of some of these resources can be found
in [Related documentation](manual.md#related-documentation) section.

# Instantiation functions

LuaLDAP provides some ways to create a LDAP connection object:

### `lualdap.open_simple (hostname, who, password, usetls, timeout)`

Initializes a session with an LDAP server.

The argument `hostname` may contain a blank-separated list of hosts
to try to connect to, and each host may optionally by of the form _host:port_.

The argument `who` should be the [distinguished name](manual.md#distinguished-names)
of the entry that has the password to be checked against
the third argument, `password`.

The optional argument `usetls` is a boolean flag indicating
if Transport Layer Security (TLS) should be used.

The optional argument `timeout` is the connection timeout in seconds.
The precision is microseconds. It also sets a timeout for subsequent network
operations. This argument has no effect on Microsoft Windows.

Returns a connection object if the operation was successful.
In case of error it returns `nil` followed by an error string.

### `lualdap.open (hostname, usetls, timeout)`

Open and initialize a connection to a LDAP server (without binding, see method `bind_simple`).

The argument `hostname` may contain a blank-separated list of hosts
to try to connect to, and each host may optionally by of the form _host:port_.

The optional argument `usetls` is a boolean flag indicating
if Transport Layer Security (TLS) should be used.

The optional argument `timeout` is the connection timeout in seconds.
The precision is microseconds. It also sets a timeout for subsequent network
operations. This argument has no effect on Microsoft Windows.

Returns a connection object if the operation was successful.
In case of error it returns `nil` followed by an error string.

### `lualdap.initialize (uri)` **DEPRECATED** in favor of `open`

Open and initialize a connection to a server (without binding, see method `bind_simple`).

The `uri` parameter may be a comma- or whitespace-separated list of
URIs containing only the schema, the host, and the port fields.

Returns a connection object if the operation was successful.

# Connection objects

A connection object offers methods which implement LDAP operations.
Almost all of them need a [distinguished name](manual.md#distinguished-names)
to identify the entry on which the operation will be executed.

These methods execute asynchronous operations and return a function
that should be called to obtain the results.
The called functions will return `true` indicating the success of the operation.
The only exception is the `compare` function which can return
either `true` or `false` (as the result of the comparison) on a successful operation.

There are two types of errors:
**API errors**, such as wrong parameters, absent connection etc.;
and **LDAP errors**, such as malformed DN, unknown attribute etc.
API errors will raise a Lua error,
while LDAP errors will be reported by the function/method returning `nil`
plus the error message provided by the OpenLDAP client.

A connection object can be created by calling a
[Instantiation function](manual.md#instantiation-functions).

## Methods

### `conn:add (distinguished_name, table_of_attributes)`

Adds a new entry to the directory with the given attributes and values.

### `conn:bind_simple (who, password)`

Bind to the directory.

The argument `who` should be the [distinguished name](manual.md#distinguished-names)
of the entry that has the password to be checked against
the second argument, `password`.

Returns the connection object if the operation was successful.
In case of error it returns `nil` followed by an error string.

### `conn:close ()`

Closes the connection `conn`.

Returns `1` in case of success; nothing when already closed.

### `conn:compare (distinguished_name, attribute, value)`

Compares a value to an entry.

### `conn:delete (distinguished_name)`

Deletes an entry from the directory.

### `conn:modify (distinguished_name, table_of_operations*)`

Changes the values of attributes in the given entry.
The tables of operations are [tables of attributes](manual.md#representing-attributes)
with the value on index `1` indicating the operation to be performed.
The valid operations are: 

- `+` to add the values to the attributes
- `-` to delete the values of the attributes
- `=` to replace the values of the attributes

Any number of tables of operations will be used in a single LDAP modify operation.

### `conn:rename (distinguished_name, new_relative_dn, new_parent, delete_old)`

Changes an entry name (i.e. change its [distinguished name](manual.md#distinguished-names)).

The optional argument `new_parent` is a [distinguished name](manual.md#distinguished-names),
without it only the RDN is changed.

The optional argument `delete_old` is an integer. With the default value `0`,
old RDN should be retained, otherwise old RDN should be deleted.

### `conn:search (table_of_search_parameters)`

Performs a search operation on the directory.
The parameters are described below:

-    `attrs`

     a string or a list of attribute names to be retrieved (default is to retrieve all attributes).

-    `attrsonly`

     a boolean value that must be either `false` (default)
     if both attribute names and values are to be retrieved,
     or `true` if only names are wanted.

-    `base`

     The [distinguished name](manual.md#distinguished-names) of the entry at which to start the search.

-    `filter`

     A string representing the search filter as described in
     [String Representation of LDAP Search Filters](https://tools.ietf.org/html/rfc4515)

-    `scope`

     A string indicating the scope of the search.
     The valid strings are: `base`, `onelevel` and `subtree`.
     The empty string (`""`) and `nil` will be treated as the default scope.

-    `sizelimit`

     The maximum number of entries to return (default is no limit).

-    `timeout`

      The timeout in seconds (default is no timeout).
      The precision is microseconds.

The search method will return a _search iterator_ which is a function
that requires no arguments.
The search iterator is used to get the search result
and will return a string representing the
[distinguished name](manual.md#distinguished-names)
and a [table of attributes](manual.md#representing-attributes)
as returned by the search request.

# Example

Here is a some sample code that demonstrate the basic use of the library (see also the 
[Teal type definition](https://github.com/teal-language/teal-types/blob/master/types/lualdap/lualdap.d.tl) of the library).

```lua
local lualdap = require"lualdap"

local ld = assert(lualdap.open_simple("ldap.server",
                "mydn=manoeljoaquim,ou=people,dc=ldap,dc=world",
                "mysecurepassword"))

for dn, attribs in ld:search{ base = "ou=people,dc=ldap,dc=world", scope = "subtree" } do
    io.write("\t[" .. dn .. "]\n")
    for name, values in pairs(attribs) do
        io.write("[" .. name .. "] : ")
        if type(values) == "string" then
            io.write(values)
        elseif type(values) == "table" then
            local n = #values
            for i = 1, n-1 do
                io.write(values[i] .. ",")
            end
            io.write(values[n])
        end
        io.write("\n")
    end
end

ld:add("mydn=newuser,ou=people,dc=ldap,dc=world", {
    objectClass = { "", "", },
    mydn = "newuser",
    abc = "qwerty",
    tel = { "123456758", "98765432", },
    givenName = "New User",
})()

ld:modify("mydn=newuser,ou=people,dc=ldp,dc=world",
    { '=', givenName = "New", cn = "New", sn = "User", },
    { '+', o = { "University", "College", },
           mail = "newuser@university.edu", },
    { '-', abc = "True", tel = "123456758", },
    { '+', tel = "13579113", }
)()

ld:delete("mydn=newuser,ou=people,dc=ldp,dc=world")()
```

# Related documentation

- [LDAP: Technical Specification Road Map](https://tools.ietf.org/html/rfc4510) (RFC4510)
- [LDAP: The Protocol](https://tools.ietf.org/html/rfc4511) (RFC4511)
- [LDAP: Directory Information Models](https://tools.ietf.org/html/rfc4512) (RFC4512)
- [LDAP: Authentication Methods and Security Mechanisms](https://tools.ietf.org/html/rfc4513) (RFC4513)
- [LDAP: String Representation of Distinguished Names](https://tools.ietf.org/html/rfc4514) (RFC4514)
- [LDAP: String Representation of Search Filters](https://tools.ietf.org/html/rfc4515) (RFC4515)
- [LDAP: Uniform Resource Locator](https://tools.ietf.org/html/rfc4516) (RFC4516)
- [LDAP: Syntaxes and Matching Rules](https://tools.ietf.org/html/rfc4517) (RFC4517)
- [LDAP: Internationalized String Preparation](https://tools.ietf.org/html/rfc4518) (RFC4518)
- [LDAP: Schema for User Applications](https://tools.ietf.org/html/rfc4519) (RFC4519)
- [The C LDAP Application Program Interface](https://www.ietf.org/proceedings/51/I-D/draft-ietf-ldapext-ldap-c-api-05.txt) (draft IETF)
- [OpenLDAP API](https://openldap.org/software/man.cgi?query=ldap)
- [WinLDAP API](https://docs.microsoft.com/en-us/windows/win32/api/_ldap/)

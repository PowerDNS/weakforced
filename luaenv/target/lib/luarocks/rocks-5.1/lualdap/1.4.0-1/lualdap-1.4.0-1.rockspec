package = 'lualdap'
version= '1.4.0-1'
source = {
    url = 'git://github.com/lualdap/lualdap',
    tag = 'v1.4.0',
}
description = {
    summary = "A Lua interface to the OpenLDAP library",
    detailed = [[
       LuaLDAP is a simple interface from Lua to an LDAP client, in
       fact it is a bind to OpenLDAP. It enables a Lua program to
       connect to an LDAP server; execute any operation (search, add,
       compare, delete, modify and rename); retrieve entries and
       references of the search result.
    ]],
    license = 'MIT',
    homepage = 'https://lualdap.github.io/lualdap/',
}
dependencies = {
    'lua >= 5.1'
}
external_dependencies = {
    platforms = {
        unix = {
            LDAP = {
                header = 'ldap.h',
                library = 'ldap',
            },
        },
    }
}
build = {
    type = 'builtin',
    modules = {
        lualdap = {
            sources = { 'src/lualdap.c' },
        },
    },
    platforms = {
        unix = {
            modules = {
                lualdap = {
                    libdirs = { '$(LDAP_LIBDIR)' },
                    incdirs = { '$(LDAP_INCDIR)' },
                    libraries = { 'ldap', 'lber' },
                },
            },
        },
        windows = {
            modules = {
                lualdap = {
                    defines = { 'WIN32', 'WINLDAP' },
                    libraries = { 'wldap32' },
                },
            },
        },
    },
    copy_directories = { 'docs' },
}

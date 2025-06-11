#!/bin/bash
#
# Build a LuaJIT env that allows you to run filter code locally on macOS and Linux.
#
# It can also build a binary distribution that can be unpacked into a prefix 
# like /usr/share/wforce-lua-dist
# 
# The destination can be configured using the -p parameter, but the resulting files 
# cannot be moved after the build. A change of location requires a rebuild.
#

# Dir of script
SCRIPTROOT="$( cd "$(dirname "$0")" ; pwd -P )"
# Target installation dir
PREFIX="$SCRIPTROOT/target"
# Indicates this is a devenv
DEVENV=yes
# Source build dir
SRCDIR="$SCRIPTROOT/src"
# Repo dir
PROJECTDIR="$SCRIPTROOT/.."

# Exit on error
set -e
# Error on unbound variable
set -u
# Error if any command in a pipe fails, not just the last one
set -o pipefail

usage() {
    echo "Builds a binary distribution of our Lua env meant to be installed into"
    echo "a unique prefix like /usr/share/wforce-lua-dist."
    echo "By default it builds into a subdirectory of this script."
    echo
    echo "The destination can be configured using the -p parameter, but the resulting files"
    echo "cannot be moved after the build. A change of location requires a rebuild."
    echo
    echo "USAGE:    $0 [OPTIONS]"
    echo
    echo "Options:"
    echo
    echo "  -p       - Install prefix (default: $PREFIX)"
    echo
    exit 1
}

while getopts ":p:hs" opt; do
    case $opt in
    p)  PREFIX="$OPTARG"
        DEVENV=no
        ;;
    h)  usage
        ;;
    \?) echo "Invalid option: -$OPTARG" >&2
        usage
        ;;
    :)  echo "Missing required argument for -$OPTARG" >&2
        usage
        ;;
    esac
done
shift $((OPTIND-1))

BINDIR="$PREFIX/bin"

if [ "$(uname)" = "Darwin" ]; then
    libext=dylib
else
    libext=so
fi


echo "Source build dir: $SRCDIR"
echo "Installing into target prefix: $PREFIX"
echo "This is a devenv: $DEVENV"

mkdir -p "$SRCDIR"

SECTION() {
    echo
    echo "------------------------------------------------------------------------------"
    echo "--- $1"
    echo "------------------------------------------------------------------------------"
    echo
}

download_unpack_cd() {
    url="$1"
    archive="$2"
    dir="$3"
    if [ ! -f "$archive" ]; then
        echo "Downloading $url"
        curl -L -o "$archive" "$url"
    fi
    if [ ! -d "$dir" ]; then
        if [ "${archive: -4}" == ".zip" ]; then
            unzip "$archive"
        else
            tar -xvzf "$archive"
        fi
    fi
    cd "$dir"
}

###########################################################################

SECTION "Install luajit"

# This uses the OpenResty LuaJIT fork
#luajit_version="2.1-20240314"
luajit_version="2.1-20230410"
luajit_tarfile="v${luajit_version}.tar.gz"
luajit_tardir="luajit2-${luajit_version}"
luajit_url="https://github.com/openresty/luajit2/archive/refs/tags/${luajit_tarfile}"
pushd "$SRCDIR"
download_unpack_cd "$luajit_url" "$luajit_tarfile" "$luajit_tardir"
# Changes `jit.version` and `luajit -v` output
sed -i.bak "s/^#define LUAJIT_VERSION.*\"LuaJIT 2.1.0-beta3\"/#define LUAJIT_VERSION \"LuaJIT 2.1.0-beta3 openresty-fork=${luajit_version} wforce-lua-dist\"/" src/luajit.h
MACOSX_DEPLOYMENT_TARGET="10.14" CFLAGS="-DLUAJIT_ENABLE_GC64" make -j install PREFIX="$PREFIX"
popd
pushd "$BINDIR"
ln -sf luajit-2.1.0-beta3 luajit
popd

###########################################################################

SECTION "Install luarocks package manager"

luarocks_version="3.12.0"
luarocks_tarfile="luarocks-${luarocks_version}.tar.gz"
luarocks_tardir="luarocks-${luarocks_version}"
luarocks_url="https://luarocks.org/releases/${luarocks_tarfile}"
pushd "$SRCDIR"
download_unpack_cd "$luarocks_url" "$luarocks_tarfile" "$luarocks_tardir"
./configure --with-lua="$PREFIX" --with-lua-interpreter=luajit --rocks-tree="$PREFIX" --prefix="$PREFIX"
make -j
make install
popd

###########################################################################

SECTION "openssl DIR for luasec"

# Check for macOS homebrew to make luasec happy
OPENSSL_DIR=${OPENSSL_DIR:-}
for cellar in /opt/homebrew/Cellar /usr/local/Cellar ; do
    if [ -z "$OPENSSL_DIR" ] && [ -d "$cellar" ] ; then
        for o in $cellar/openssl* ; do
            for v in $o/* ; do
                echo "Found OPENSSL_DIR=$v"
                export OPENSSL_DIR="$v"
            done
        done
        echo
    fi
done

###########################################################################

SECTION "Install luarocks dependencies"

export LUAROCKS="$BINDIR/luarocks"

# We touch files to indicate we installed them
installed_rocks="$SRCDIR/installed-luarocks"
mkdir -p "$installed_rocks"

CFLAGS_bak="${CFLAGS:-}"

IFS=$'\n'   # make newlines the only separator
for p in $( cat "luarocks.list" | grep -v '^#' ); do
    unset IFS   # restore default IFS for loop contents
    
    echo
    echo "==> Installing $p"

    name=$(basename $p)  # ignores version
    installed_marker="$installed_rocks/$name"
    if [ -f "$installed_marker" ]; then
        echo "Already installed (remove $installed_marker to reinstall)"
        continue
    fi

    install_args=""
    if [ "$name" = "luasec" ] && [ -n "$OPENSSL_DIR" ] ; then
        install_args="OPENSSL_DIR=$OPENSSL_DIR"
    fi

    # Actual package install
    if ! $LUAROCKS install $p $install_args; then
        echo "ERROR: could not install $p"
        exit 3
    fi

    touch "$installed_marker"
    
    # Undo workarounds
    if [ "$p" = "lua-cjson-ol" ] ; then
        mv "$PREFIX/include/luajit-2.1/lauxlib.h.bak" "$PREFIX/include/luajit-2.1/lauxlib.h"
    fi
    if [ "$name" = "busted" ]; then
        export CFLAGS="$CFLAGS_bak"
    fi

    IFS=$'\n'   # make newlines the only separator
done
unset IFS


###########################################################################

SECTION "Runtime environment variables"

# The only reason we set these is in case a different LuaJIT is used with this env, e.g. in OpenResty.
LUA_PATH="$PREFIX/share/lua/5.1/?.lua;$PREFIX/share/lua/5.1/?/init.lua;;"
LUA_PATH_DEVENV="$PROJECTDIR/?.lua;$PROJECTDIR/?/init.lua;$LUA_PATH"
if [ "$DEVENV" = "yes" ]; then
    LUA_PATH="$LUA_PATH_DEVENV"
fi

tee "$PREFIX/bin/activate.sh" <<-EOF
export PATH="$PREFIX/bin:\$PATH"
export LUAROCKS="$PREFIX/bin/luarocks"
export CFLAGS="-I$PREFIX/include/luajit-2.1"
export LDFLAGS="-L$PREFIX/lib"
export CGO_CFLAGS="-I$PREFIX/include/luajit-2.1"
export CGO_LDFLAGS="-L$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export LUA_PATH="$LUA_PATH"
export LUA_CPATH="$PREFIX/lib/lua/5.1/?.so;;"
export PDNS_PF_FFI_LIB="$PREFIX/ffilibs"
EOF

echo
echo "The file $PREFIX/bin/activate.sh contains the env vars you need to set"
echo

echo "For https://github.com/wojas/envy add this to the .envy file in the repo root instead:"
echo
echo "ENVY_EXTEND_PATH=$PREFIX/bin"
cat "$PREFIX/bin/activate.sh" | grep -v "export PATH=" | sed 's/^export //; s/"//g' 
echo

###########################################################################

SECTION "Run tests"

source "$PREFIX/bin/activate.sh"
LUA_PATH="$LUA_PATH_DEVENV" luajit test.lua

if [ "$DEVENV" = "yes" ]; then
    ln -sf "target/bin" "$SCRIPTROOT/bin"
fi


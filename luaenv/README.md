This sets up a devenv Lua environment locally.

It is also used to build the production Lua distribution when the `-p` flag is used.

It downloads and builds luajit & luarocks 
and installs them under this directory without polluting your global install.

Currently only tested on macOS Big Sur and Ubuntu 20.04, but it should work on most Linux distributions.

Requirements (general):

- working C compilation toolchain
- curl

Requirements (macOS):

- brew install openssl


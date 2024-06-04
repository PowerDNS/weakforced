%define luadist_root /usr/share/wforce-lua-dist

Name:           wforce-lua-dist
Version:        %{getenv:BUILDER_RPM_VERSION}
Release:        %{getenv:BUILDER_RPM_RELEASE}%{dist}
Summary:        PowerDNS Filtering Platform LuaJIT distribution
License:        mixed
URL:            https://www.powerdns.com/
BuildArch:      x86_64
Provides:       luajit = 2.1.0-0.16beta3.openresty.%{getenv:BUILDER_RPM_RELEASE}%{dist}
BuildRequires:  openldap-devel

%description
PowerDNS Weakforced LuaJIT distribution

# We'll not provide this, on purpose
# No package in Fedora shall ever depend on this
# We do provide the libs
%global __provides_exclude ^%{luadist_root}/bin/.*$
# We also do not require anything we did not explicitly specify, no magic please
%global __requires_exclude ^.*$

%post
/sbin/ldconfig

%postun
/sbin/ldconfig


%prep


%build
# This is performed in the Dockerfile:   ./build.sh -p %{luadist_root}
# Just to log what it's linked against
ldd %{luadist_root}/bin/luajit

%install
%{__install} -m 755 -d %{buildroot}/usr/share
mv %{luadist_root} %{buildroot}%{luadist_root}
find %{buildroot}

# Hacky symlink, in case needed
#ln -s %{buildroot}%{luadist_root} %{luadist_root}
#rm %{luadist_root}

# Create a few useful entrypoints in /usr/bin
%{__install} -m 755 -d %{buildroot}/usr/bin/
%{__install} -m 755 -d %{buildroot}%{_libdir}
ln -sf %{luadist_root}/bin/luajit %{buildroot}/usr/bin/wforce-luajit

# So that services find libluajit-5.1.so.2 here
%{__install} -m 755 -d %{buildroot}%{luadist_root}/lib/exported
ln -sf ../libluajit-5.1.so   %{buildroot}%{luadist_root}/lib/exported/libluajit-5.1.so
ln -sf ../libluajit-5.1.so.2 %{buildroot}%{luadist_root}/lib/exported/libluajit-5.1.so.2
%{__install} -m 755 -d %{buildroot}/etc/ld.so.conf.d
echo "%{luadist_root}/lib/exported" > %{buildroot}/etc/ld.so.conf.d/wforce-lua-dist.conf

# So that pkg-config finds us
%{__install} -m 755 -d %{buildroot}/usr/lib/pkgconfig/
ln -sf %{luadist_root}/lib/pkgconfig/luajit.pc %{buildroot}/usr/lib/pkgconfig/luajit.pc

%files
/usr/bin/wforce-luajit
/etc/ld.so.conf.d/wforce-lua-dist.conf
/usr/lib/pkgconfig/luajit.pc
%{luadist_root}


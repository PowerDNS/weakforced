%if %{?fedora}0 > 140 || %{?rhel}0 > 60 || %{?centos}0 > 60
%bcond_without systemd
%else
%bcond_with systemd
%endif

%define venv_cmd %{__python3} -m venv --symlinks
%define venv_name %{name}-report-api
%define venv_install_dir /usr/share/%{venv_name}
%define venv_dir %{buildroot}%{venv_install_dir}
%define venv_bin %{venv_dir}/bin
%define venv_python %{venv_bin}/python
%define venv_pip %{venv_python} %{venv_bin}/pip install
%define configdir /etc/%{venv_name}
%define vardir /var/lib/%{venv_name}
%define __prelink_undo_cmd %{nil}
%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')


%global wforce_restart_flag /var/run/wforce-restart-after-rpm-install
%global trackalert_restart_flag /var/run/wforce-trackalert-restart-after-rpm-install

Summary: Weakforce daemon for detecting brute force attacts
Name: wforce
Version: %{getenv:BUILDER_RPM_VERSION}
Release: %{getenv:BUILDER_RPM_RELEASE}
License: GPLv3
Group: System Environment/Daemons
URL: http://www.open-xchange.com/
Source0: %{name}-%{getenv:BUILDER_VERSION}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: libtool
BuildRequires: automake
BuildRequires: readline-devel
BuildRequires: gcc-c++
BuildRequires: lua-devel
BuildRequires: libidn-devel
BuildRequires: libuv-devel
BuildRequires: GeoIP-devel
BuildRequires: libmaxminddb-devel
BuildRequires: boost-devel 
BuildRequires: bzip2
BuildRequires: pkgconfig
BuildRequires: getdns-devel
BuildRequires: libsodium-devel
BuildRequires: pandoc
BuildRequires: protobuf-compiler
BuildRequires: protobuf-devel
BuildRequires: curl-devel
BuildRequires: luajit
BuildRequires: luajit-devel
BuildRequires: hiredis
BuildRequires: hiredis-devel
BuildRequires: openssl-devel
BuildRequires: yaml-cpp-devel
BuildRequires: boost-regex
BuildRequires: wget
BuildRequires: boost-system
BuildRequires: boost-filesystem
%if %{with systemd}
BuildRequires: systemd-devel
Requires: systemd
Requires(postun): systemd
%else
Requires: initscripts
Requires(postun): /sbin/service
%endif
%if %{?centos} == 7 || %{?rhel} == 7
BuildRequires: devtoolset-7-gcc-c++
%define scl devtoolset-7
%endif
AutoReqProv: yes

%description
 The goal of 'wforce' is to detect brute forcing of passwords across many
 servers, services and instances. In order to support the real world, brute
 force detection policy can be tailored to deal with "bulk, but legitimate"
 users of your service, as well as botnet-wide slowscans of passwords.
 The aim is to support the largest of installations, providing services to
 hundreds of millions of users.

%package trackalert
Summary: Longterm abuse data reporting and alerter

%description trackalert
 Trackalert is designed to be an optional service to complement
 wforce. Whereas wforce provides a toolkit to combat  abuse of
 logins such as password brute forcing in realtime, trackalert is
 designed to look at abuse asynchronously, using long-term report data
 stored in an external DB such as elasticsearch, and to send alerts on
 potential login abuse.

%package report-api
Summary: Enable access to the report information stored in Elasticsearch.
Requires: python%{python3_pkgversion}-setuptools
Requires: python%{python3_pkgversion}-devel
Requires: python%{python3_pkgversion}-pip

%description report-api
 The Report API is provided to enable access to the report information stored in Elasticsearch.
 It provides REST API endpoints to retrieve data about logins and devices, as well as endpoints to "forget" devices and logins.
%prep
%setup -n %{name}-%{getenv:BUILDER_VERSION}

%build
%{?scl:scl enable %{scl} - << \EOF}
%configure                       \
    --disable-dependency-tracking \
    --docdir=%{_docdir}/%{name}-%{getenv:BUILDER_VERSION} \
    --disable-static --with-luajit --sysconfdir=/etc/%{name} \
    --enable-trackalert
%{?scl:EOF}

%{?scl:scl enable %{scl} - << \EOF}
make %{?_smp_mflags}
%{?scl:EOF}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
mkdir -p %{buildroot}/%{_docdir}/%{name}-%{version}
mv %{buildroot}/etc/%{name}/%{name}.conf.example %{buildroot}/%{_docdir}/%{name}-%{version}/
mv elk/logstash/config/logstash.conf %{buildroot}/%{_docdir}/%{name}-%{version}/
mv elk/logstash/templates/wforce_template.json %{buildroot}/%{_docdir}/%{name}-%{version}/
mv elk/kibana/kibana_saved_objects.json %{buildroot}/%{_docdir}/%{name}-%{version}/

%{venv_cmd} %{venv_dir}
%{venv_pip} -U pip setuptools
pushd report_api
%{venv_pip} .
popd

# RECORD files are used by wheels for checksum. They contain path names which
# match the buildroot and must be removed or the package will fail to build.
find %{buildroot} -name "RECORD" -exec rm -rf {} \;
find %{venv_dir} -type f -iname '*.pyc' -delete
find %{venv_dir} -type d -iname '__pycache__' -delete

# Change the virtualenv path to the target installation directory.
%{venv_pip} -U venvctrl
%{venv_bin}/venvctrl-relocate --source=%{venv_dir} --destination=%{venv_install_dir}
%{venv_python} %{venv_bin}/pip3 uninstall --yes venvctrl

#remove unfixable files and pycache
%{__rm} -f %{venv_dir}/bin/activate.*

# Strip native modules as they contain buildroot paths in their debug information
find %{venv_dir}/lib -type f -name "*.so" | xargs -r strip

# Remove the venvdir from all file contents
find %{venv_dir} -type f -exec sed -e 's!%{venv_dir}!%{venv_install_dir}!g' -i {} +

# install some files for the report-api
%{__install} -D -m 755 report_api/helpers/wforce-report-api-webserver %{buildroot}/%{_bindir}/wforce-report-api-webserver
%{__install} -D -m 644 report_api/helpers/wforce-report-api.conf %{buildroot}/%{_sysconfdir}/%{name}-report-api/wforce-report-api-web.conf
%{__install} -D -m 644 report_api/instance/report.cfg %{buildroot}/%{_sysconfdir}/%{name}-report-api/%{name}-report-api-instance.conf
%{__install} -D -m 644 report_api/helpers/wforce-report-api.service %{buildroot}/%{_unitdir}/wforce-report-api.service
sed -i -e s:\<python_version\>:%{python3_version}: %{buildroot}/%{_sysconfdir}/%{name}-report-api/wforce-report-api-web.conf

%clean
rm -rf %{buildroot}

%pre trackalert
if [ "$1" = "2" ] || [ "$1" = "1" ]; then
  rm -f %trackalert_restart_flag
%if %{with systemd}
  /bin/systemctl is-active %{name}.service >/dev/null 2>&1 && touch %trackalert_restart_flag || :
   /bin/systemctl is-active %{name}.service >/dev/null 2>&1  && \
       /bin/systemctl stop %{name}.service >/dev/null 2>&1 || :
%endif
fi

%pre
getent group %{name} >/dev/null || groupadd -r %{name}
getent passwd %{name} >/dev/null || \
useradd -r -g %{name} -d /var/spool/%{name} -s /bin/false -c "wforce" %{name}

if [ "$1" = "2" ] || [ "$1" = "1" ]; then
  rm -f %wforce_restart_flag
%if %{with systemd}
  /bin/systemctl is-active %{name}.service >/dev/null 2>&1 && touch %wforce_restart_flag || :
   /bin/systemctl is-active %{name}.service >/dev/null 2>&1  && \
       /bin/systemctl stop %{name}.service >/dev/null 2>&1 || :
%else
  /sbin/service %{name} status >/dev/null 2>&1 && touch %wforce_restart_flag || :
  /sbin/service %{name} stop >/dev/null 2>&1 || :
%endif
fi

%pre report-api
getent group %{name}-report-api >/dev/null || groupadd -r %{name}-report-api
getent passwd %{name}-report-api >/dev/null || \
useradd -r -g %{name}-report-api -d /var/spool/%{name}-report-api -s /bin/false -c "wforce-report-api" %{name}-report-api

%post report-api
%if %{with systemd}
%systemd_post %{name}-report-api
%endif

%preun report-api
%if %{with systemd}
%systemd_preun %{name}-report-api
%endif

%postun report-api
%if %{with systemd}
%systemd_postun_with_restart %{name}-report-api
%endif

%post trackalert
# Post-Install
if [ $1 -eq 1 ]; then
  TRACKALERTCONF=/etc/wforce/trackalert.conf
  echo -n "Modifying trackalert.conf to replace password and key..."
  SETKEY=`echo "makeKey()" | wforce | grep setKey`
  WEBPWD=`dd if=/dev/urandom bs=1 count=32 2>/dev/null | base64 | rev | cut -b 2-14 | rev`
  sed -e "s#--WEBPWD#$WEBPWD#" -e "s#--SETKEY#$SETKEY#" -i $TRACKALERTCONF
  echo "done"

%if %{with systemd}
  systemctl daemon-reload
  systemctl enable trackalert.service
%endif
fi

%post
# Post-Install
if [ $1 -eq 1 ]; then
  WFORCECONF=/etc/%{name}/%{name}.conf
  echo -n "Modifying %{name}.conf to replace password and key..."
  SETKEY=`echo "makeKey()" | %{name} | grep setKey`
  WEBPWD=`dd if=/dev/urandom bs=1 count=32 2>/dev/null | base64 | rev | cut -b 2-14 | rev`
  sed -e "s#--WEBPWD#$WEBPWD#" -e "s#--SETKEY#$SETKEY#" -i $WFORCECONF
  echo "done"

%if %{with systemd}
  systemctl daemon-reload
  systemctl enable %{name}.service
%else
  chkconfig --add %{name}
%endif
fi
# Post-Upgrade
if [ $1 -eq 2 ]; then
   if [ -e "/etc/%{name}.conf" ]; then
      echo "Found old config file /etc/%{name}.conf. Copying it to /etc/%{name}/%{name}.conf"
      if [ -e "/etc/%{name}/%{name}.conf" ]; then
         echo "Found also new config file /etc/%{name}/%{name}.conf"
         mv /etc/%{name}/%{name}.conf /etc/%{name}/%{name}.conf.backup.$$ && \
         echo "Renamed /etc/%{name}/%{name}.conf to /etc/%{name}/%{name}.conf.backup.$$"
      fi
      cp -a /etc/%{name}.conf /etc/%{name}.conf.backup.$$ && \
      echo "Renamed old /etc/%{name}.conf to /etc/%{name}.conf.backup.$$"
      mv /etc/%{name}.conf /etc/%{name}/%{name}.conf && \
      echo "Moved /etc/%{name}.conf to /etc/%{name}/%{name}.conf"
   fi
fi

%preun trackalert
if [ $1 = 0 ]; then
%if %{with systemd}
    /bin/systemctl disable trackalert.service trackalert.socket >/dev/null 2>&1 || :
    /bin/systemctl stop trackalert.service trackalert.socket >/dev/null 2>&1 || :
%endif
fi


%preun
if [ $1 = 0 ]; then
%if %{with systemd}
    /bin/systemctl disable %{name}.service %{name}.socket >/dev/null 2>&1 || :
    /bin/systemctl stop %{name}.service %{name}.socket >/dev/null 2>&1 || :
%else
    /sbin/service %{name} stop > /dev/null 2>&1
    /sbin/chkconfig --del %{name}
%endif
fi


%postun trackalert
%if %{with systemd}
/bin/systemctl daemon-reload >/dev/null 2>&1 || :
%endif

if [ "$1" -ge "1" -a -e %trackalert_restart_flag ]; then
%if %{with systemd}
    /bin/systemctl start trackalert.service >/dev/null 2>&1 || :
%endif
rm -f %trackalert_restart_flag
fi

%postun
%if %{with systemd}
/bin/systemctl daemon-reload >/dev/null 2>&1 || :
%endif

if [ "$1" -ge "1" -a -e %wforce_restart_flag ]; then
%if %{with systemd}
    /bin/systemctl start %{name}.service >/dev/null 2>&1 || :
%else
    /sbin/service %{name} start >/dev/null 2>&1 || :
%endif
rm -f %wforce_restart_flag
fi

%posttrans trackalert
# trackalert should be started again in %postun, but it's not executed on reinstall
# if it was already started, restart_flag won't be here, so it's ok to test it again
if [ -e %trackalert_restart_flag ]; then
%if %{with systemd}
    /bin/systemctl start trackalert.service >/dev/null 2>&1 || :
%endif
rm -f %trackalert_restart_flag
fi

%posttrans
# %{name} should be started again in %postun, but it's not executed on reinstall
# if it was already started, wforce_restart_flag won't be here, so it's ok to test it again
if [ -e %wforce_restart_flag ]; then
%if %{with systemd}
    /bin/systemctl start %{name}.service >/dev/null 2>&1 || :
%else
    /sbin/service %{name} start >/dev/null 2>&1 || :
%endif
rm -f %wforce_restart_flag
fi

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_bindir}/wf_dump_entries
%ghost %{_sysconfdir}/%{name}.conf
%attr(0644,root,root) %config(noreplace,missingok) %{_sysconfdir}/%{name}/%{name}.conf
%{_sysconfdir}/%{name}/regexes.yaml
%{_docdir}/%{name}-%{version}/*
%{_unitdir}/%{name}.service
%{_mandir}/man1/%{name}.1.gz
%{_mandir}/man1/wf_dump_entries.1.gz
%{_mandir}/man5/%{name}.conf.5.gz
%{_mandir}/man5/%{name}_webhook.5.gz
%doc CHANGELOG.md README.md
%license LICENSE

%files trackalert
%{_bindir}/trackalert
%{_mandir}/man1/trackalert.1.gz
%{_mandir}/man5/trackalert.conf.5.gz
%doc trackalert/README.md CHANGELOG.md
%license LICENSE
%{_unitdir}/trackalert.service
%{_sysconfdir}/%{name}/trackalert.conf

%files report-api
%{venv_install_dir}
%attr(0644,root,root) %config(noreplace,missingok) %{_sysconfdir}/%{name}-report-api/*.conf
%{_bindir}/%{name}-report-api-webserver
%{_unitdir}/%{name}-report-api.service
%{_mandir}/man5/%{name}-report-api.5.gz

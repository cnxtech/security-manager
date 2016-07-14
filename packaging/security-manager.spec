Name:       security-manager
Summary:    Security manager and utilities
Version:    1.1.11
Release:    0
Group:      Security/Service
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    security-manager.manifest
Source3:    libsecurity-manager-client.manifest
Requires: security-manager-policy
Requires: nether
Requires(post): sqlite3
Requires(post): smack
BuildRequires: cmake
BuildRequires: zip
# BuildRequires: pkgconfig(dlog)
BuildRequires: libattr-devel
BuildRequires: pkgconfig(libprocps)
BuildRequires: pkgconfig(libsmack)
BuildRequires: pkgconfig(libcap)
BuildRequires: pkgconfig(libsystemd-daemon)
BuildRequires: pkgconfig(libsystemd-journal)
BuildRequires: pkgconfig(libtzplatform-config)
BuildRequires: tizen-platform-config-tools
BuildRequires: pkgconfig(sqlite3)
BuildRequires: pkgconfig(db-util)
BuildRequires: pkgconfig(cynara-admin)
BuildRequires: pkgconfig(cynara-client-async)
BuildRequires: pkgconfig(security-privilege-manager)
BuildRequires: boost-devel
%{?systemd_requires}

%description
Tizen security manager and utilities

%package -n libsecurity-manager-client
Summary:    Security manager (client)
Group:      Security/Libraries
Requires:   security-manager = %{version}-%{release}
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description -n libsecurity-manager-client
Tizen Security manager client library

%package -n libsecurity-manager-client-devel
Summary:    Security manager (client-devel)
Group:      Security/Development
Requires:   libsecurity-manager-client = %{version}-%{release}

%description -n libsecurity-manager-client-devel
Development files needed for using the security manager client

%package policy
Summary:    Security manager policy
Group:      Security/Access Control
Requires:   sed
Requires(post): security-manager = %{version}-%{release}
Requires(post): cyad
Requires(post): sqlite
Requires(post): tizen-platform-config-tools

%description policy
Set of security rules that constitute security policy in the system

%define TZ_SKEL_APP %(tzplatform-get TZ_USER_APP | cut -d= -f2 | sed "s|^$HOME|%{_sysconfdir}/skel|")

%prep
%setup -q
cp %{SOURCE1} .
cp %{SOURCE3} .

%build
%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif

export LDFLAGS+="-Wl,--rpath=%{_libdir}"

%cmake . -DVERSION=%{version} \
        -DBIN_INSTALL_DIR=%{_bindir} \
        -DDB_INSTALL_DIR=%{TZ_SYS_DB} \
        -DLOCAL_STATE_DIR=%{TZ_SYS_VAR} \
        -DSYSTEMD_INSTALL_DIR=%{_unitdir} \
        -DCMAKE_BUILD_TYPE=%{?build_type:%build_type}%{!?build_type:RELEASE} \
        -DCMAKE_VERBOSE_MAKEFILE=ON
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_datadir}/license
cp LICENSE %{buildroot}%{_datadir}/license/%{name}
cp LICENSE %{buildroot}%{_datadir}/license/libsecurity-manager-client
%make_install

mkdir -p %{buildroot}/%{_unitdir}/sockets.target.wants
mkdir -p %{buildroot}/%{_unitdir}/sysinit.target.wants
mkdir -p %{buildroot}/%{_unitdir}/basic.target.wants
ln -s ../security-manager.socket %{buildroot}/%{_unitdir}/sockets.target.wants/security-manager.socket
ln -s ../security-manager-cleanup.service %{buildroot}/%{_unitdir}/sysinit.target.wants/security-manager-cleanup.service
ln -s ../security-manager-rules-loader.service %{buildroot}/%{_unitdir}/basic.target.wants/security-manager-rules-loader.service

mkdir -p %{buildroot}/%{TZ_SYS_DB}
touch %{buildroot}/%{TZ_SYS_DB}/.security-manager.db
touch %{buildroot}/%{TZ_SYS_DB}/.security-manager.db-journal

install -m 0444 -D /dev/null %{buildroot}%{TZ_SKEL_APP}/apps-names
install -m 0444 -D /dev/null %{buildroot}%{TZ_SYS_RW_APP}/apps-names

%clean
rm -rf %{buildroot}

%post
/sbin/ldconfig
systemctl daemon-reload
if [ $1 = 1 ]; then
    # installation
    systemctl start security-manager.service
    %{_datadir}/security-manager/db/update.sh
fi

if [ $1 = 2 ]; then
    # update
    %{_bindir}/security-manager-migration
    systemctl restart security-manager.service
    %{_datadir}/security-manager/db/update.sh
fi

chsmack -a System %{TZ_SYS_DB}/.security-manager.db
chsmack -a System %{TZ_SYS_DB}/.security-manager.db-journal

chsmack -a _ %{TZ_SKEL_APP}/apps-names
chsmack -a _ %{TZ_SYS_RW_APP}/apps-names

%preun
if [ $1 = 0 ]; then
    # unistall
    systemctl stop security-manager.service
fi

%postun
/sbin/ldconfig
if [ $1 = 0 ]; then
    # unistall
    systemctl daemon-reload
fi

%post -n libsecurity-manager-client -p /sbin/ldconfig

%postun -n libsecurity-manager-client -p /sbin/ldconfig

%post policy
%{_bindir}/security-manager-policy-reload

%files -n security-manager
%manifest security-manager.manifest
%defattr(-,root,root,-)
%attr(755,root,root) %{_bindir}/security-manager-migration
%attr(755,root,root) %{_bindir}/security-manager
%attr(755,root,root) %{_bindir}/security-manager-cmd
%attr(755,root,root) %{_bindir}/security-manager-cleanup
%attr(755,root,root) %{_sysconfdir}/gumd/useradd.d/50_security-manager-add.post
%attr(755,root,root) %{_sysconfdir}/gumd/userdel.d/50_security-manager-remove.pre
%config(noreplace) %attr(444,root,root) %{TZ_SKEL_APP}/apps-names
%config(noreplace) %attr(444,root,root) %{TZ_SYS_RW_APP}/apps-names
%dir %attr(700,root,root) %{TZ_SYS_VAR}/security-manager/rules
%dir %attr(700,root,root) %{TZ_SYS_VAR}/security-manager/rules-merged

%{_libdir}/libsecurity-manager-commons.so.*
%attr(-,root,root) %{_unitdir}/security-manager.*
%attr(-,root,root) %{_unitdir}/security-manager-cleanup.*
%attr(-,root,root) %{_unitdir}/security-manager-rules-loader.service
%attr(-,root,root) %{_unitdir}/basic.target.wants/security-manager-rules-loader.service
%attr(-,root,root) %{_unitdir}/sockets.target.wants/security-manager.*
%attr(-,root,root) %{_unitdir}/sysinit.target.wants/security-manager-cleanup.*
%config(noreplace) %attr(0600,root,root) %{TZ_SYS_DB}/.security-manager.db
%config(noreplace) %attr(0600,root,root) %{TZ_SYS_DB}/.security-manager.db-journal
%{_datadir}/license/%{name}

%{_datadir}/security-manager/db
%attr(755,root,root) %{_datadir}/%{name}/db/update.sh
%attr(755,root,root) %{_sysconfdir}/opt/upgrade/240.security-manager.db-update.sh

%files -n libsecurity-manager-client
%manifest libsecurity-manager-client.manifest
%defattr(-,root,root,-)
%{_libdir}/libsecurity-manager-client.so.*
%{_datadir}/license/libsecurity-manager-client

%files -n libsecurity-manager-client-devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/libsecurity-manager-client.so
%{_libdir}/libsecurity-manager-commons.so
%{_includedir}/security-manager/*.h
%{_libdir}/pkgconfig/security-manager.pc

%files -n security-manager-policy
%manifest %{name}.manifest
%{_datadir}/security-manager/policy
%attr(755,root,root) %{_bindir}/security-manager-policy-reload

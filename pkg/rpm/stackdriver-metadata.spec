%define _prefix /opt/stackdriver/metadata
%define _initddir /etc/rc.d/init.d

Summary: Stackdriver metadata collection daemon
Name: stackdriver-metadata
Version: %{version}
Release: %{release}
License: Apache Software License 2.0
Group: System Environment/Daemons
Requires: yajl, boost-system >= 1.54.0, boost-thread >= 1.54.0

%description
The Stackdriver metadata daemon collects resource metadata and
sends it to the Stackdriver service.

%files
%{_sbindir}/metadatad
%{_initddir}/%{name}

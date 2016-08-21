Summary: Nixia traffic generator
Name: csperf
Version: 0.5
Packager: Nikhil <niks3089@gmail.com>
Group: Network traffic generator 
License: GPL

%define build_timestamp %(date +"%Y%b%d%H%m%%S ")

Release:  %{build_timestamp}

%description
Brief description of software package.

%prep

%build

%install
mkdir -p %_topdir/BUILDROOT/csperf-%{version}-%{release}.%_target_cpu/usr/local/bin
mkdir -p %_topdir/BUILDROOT/csperf-%{version}-%{release}.%_target_cpu/etc/csperf
mkdir -p %_topdir/BUILDROOT/csperf-%{version}-%{release}.%_target_cpu/usr/local/share/man/man1/

cp -rf %_mypath/build/csperf %_topdir/BUILDROOT/csperf-%{version}-%{release}.%_target_cpu/usr/local/bin/
cp -rf %_mypath/src/common/log.conf %_topdir/BUILDROOT/csperf-%{version}-%{release}.%_target_cpu/etc/csperf/
cp -rf %_mypath/docs/csperf.1 %_topdir/BUILDROOT/csperf-%{version}-%{release}.%_target_cpu/usr/local/share/man/man1/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)

%attr(755, root, root) /usr/local/bin/csperf
%attr(777, root, root) /etc/csperf/log.conf
%attr(644, root, root) /usr/local/share/man/man1/csperf.1

%doc

%changelog

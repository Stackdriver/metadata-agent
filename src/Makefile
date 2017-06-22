LIBDIR=../lib
CPP_NETLIB_DIR=$(LIBDIR)/cpp-netlib
CPP_NETLIB_LIBDIR=$(CPP_NETLIB_DIR)/libs/network/src
YAML_CPP_DIR=$(LIBDIR)/yaml-cpp
YAML_CPP_LIBDIR=$(YAML_CPP_DIR)

SED=/usr/bin/env sed
CMAKE=cmake
CXXFLAGS=-std=c++11 -g -Wno-deprecated-declarations -I/usr/local/include -I$(CPP_NETLIB_DIR) -I$(YAML_CPP_DIR)/include
LDFLAGS=-L/usr/local/lib -L$(CPP_NETLIB_LIBDIR) -L$(YAML_CPP_LIBDIR)
LDLIBS=-lcppnetlib-uri -lcppnetlib-client-connections -lboost_system -lboost_thread -lpthread -lyajl -lssl -lcrypto -lyaml-cpp

SRCS=\
     metadatad.cc \
     api_server.cc \
     configuration.cc \
     updater.cc \
     docker.cc \
     resource.cc \
     oauth2.cc \
     logging.cc \
     local_stream_http.cc \
     local_stream_delegate.cc \
     json.cc \
     time.cc \
     base64.cc \
     format.cc \
     environment.cc
OBJS=$(SRCS:%.cc=%.o)

CPP_NETLIB_LIBS=\
    $(CPP_NETLIB_LIBDIR)/libcppnetlib-uri.a \
    $(CPP_NETLIB_LIBDIR)/libcppnetlib-client-connections.a
YAML_CPP_LIBS=\
    $(YAML_CPP_LIBDIR)/libyaml-cpp.a
LIBS=$(CPP_NETLIB_LIBS) $(YAML_CPP_LIBS)

sbindir=/opt/stackdriver/metadata/sbin
INSTALL=/usr/bin/install
INSTALL_PROGRAM=$(INSTALL)
INSTALL_DATA=$(INSTALL) -m 644

metadatad: $(LIBS) $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

$(OBJS): init-submodules

install: metadatad
	$(INSTALL) -d $(DESTDIR)$(sbindir) && \
	$(INSTALL_PROGRAM) -t $(DESTDIR)$(sbindir) $^

export DISTRO
PKG_NAME=stackdriver-metadata
PKG_VERSION=0.0.10
PKG_RELEASE=1
PKG_MAINTAINER=Stackdriver Engineering <engineering@stackdriver.com>

PKG_TMPDIR=$$(mktemp -d --tmpdir metadata-build.XXXXXXXXXX)

DEB_PKG=$(PKG_NAME)_$(PKG_VERSION)-$(PKG_RELEASE)$(if $(DISTRO),.$(DISTRO),).deb
RPM_PKG=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_RELEASE)$(if $(DISTRO),.$(DISTRO),).x86_64.rpm

$(DEB_PKG): metadatad $(wildcard ../pkg/deb/debian/*) Makefile
	export TMPDIR=$(PKG_TMPDIR) && \
	export DEB_DIR=$$TMPDIR/deb && \
	mkdir -p $$DEB_DIR/package && \
	$(MAKE) DESTDIR=$$DEB_DIR/package install && \
	$(INSTALL) -d $$DEB_DIR/package/DEBIAN && \
	touch $$DEB_DIR/package/DEBIAN/files && \
	(echo "$(PKG_NAME) ($(PKG_VERSION)-$(PKG_RELEASE)) trusty xenial squeeze wheezy jessie; urgency=low" >> $$DEB_DIR/changelog) && \
	(echo >> $$DEB_DIR/changelog) && \
	(echo "  * automated build" >> $$DEB_DIR/changelog) && \
	(echo >> $$DEB_DIR/changelog) && \
	(echo " -- $(PKG_MAINTAINER)  $$(date -u +"%a, %d %b %Y %T %z")" >> $$DEB_DIR/changelog) && \
	(echo >> $$DEB_DIR/changelog) && \
	export DEB_CONTROL=../pkg/deb/debian/control.base && \
	if [ -f ../pkg/deb/debian/control.$${DISTRO} ]; then DEB_CONTROL=../pkg/deb/debian/control.$${DISTRO}; fi && \
	dpkg-gencontrol -v$(PKG_VERSION)-$(PKG_RELEASE) -c$$DEB_CONTROL -l$$DEB_DIR/changelog -f$$DEB_DIR/package/DEBIAN/files -Vmaintainer="$(PKG_MAINTAINER)" -P$$DEB_DIR/package -n"$@" && \
	(dpkg-deb -b $$DEB_DIR/package "$@" > $$DEB_DIR/dpkgbuild.log 2>&1) && \
	$(RM) -r $$TMPDIR

deb: $(DEB_PKG)

$(RPM_PKG): metadatad $(wildcard ../pkg/rpm/*) Makefile
	export TMPDIR=$(PKG_TMPDIR) && \
	export RPM_DIR=$$TMPDIR/rpm && \
	mkdir -p $$RPM_DIR/package && \
	$(MAKE) DESTDIR=$$RPM_DIR/package install && \
	(rpmbuild -bb --target=x86_64 -D"%_topdir $$RPM_DIR/rpmbuild" -D"%_rpmdir $(dir $@)" -D"%_rpmfilename $(notdir $@)" -D"%version $(PKG_VERSION)" -D"%release $(PKG_RELEASE)" --buildroot=$$RPM_DIR/package --noclean ../pkg/rpm/stackdriver-metadata.spec > $$RPM_DIR/rpmbuild.log 2>&1) && \
	$(RM) -r $$TMPDIR

rpm: $(RPM_PKG)

clean:
	$(RM) metadatad $(OBJS)

purge: clean
	$(RM) -r init-submodules build-cpp-netlib build-yaml-cpp \
	    $(CPP_NETLIB_DIR) $(YAML_CPP_DIR)
	git submodule deinit --all

init-submodules:
	cd .. && \
	git submodule init && \
	git submodule update
	touch init-submodules

$(CPP_NETLIB_DIR)/Makefile: init-submodules
	cd $(CPP_NETLIB_DIR) && \
	$(SED) -i -e 's/unit_test_framework //' CMakeLists.txt && \
	$(SED) -i -e 's/-Wall/-Wall -Wno-deprecated-declarations -Wno-unused-local-typedef/' CMakeLists.txt && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DCPP-NETLIB_BUILD_TESTS=OFF -DCPP-NETLIB_BUILD_EXAMPLES=OFF

$(CPP_NETLIB_LIBS): build-cpp-netlib

build-cpp-netlib: $(CPP_NETLIB_DIR)/Makefile
	cd $(CPP_NETLIB_DIR) && \
	$(MAKE) $(MAKEFLAGS)
	touch build-cpp-netlib

$(YAML_CPP_DIR)/Makefile: init-submodules
	cd $(YAML_CPP_DIR) && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DYAML_CPP_BUILD_TOOLS=OFF

$(YAML_CPP_LIBS): build-yaml-cpp

build-yaml-cpp: $(YAML_CPP_DIR)/Makefile
	cd $(YAML_CPP_DIR) && \
	$(MAKE) $(MAKEFLAGS)
	touch build-yaml-cpp

cpp-netlib: $(CPP_NETLIB_LIBS)

yaml-cpp: $(YAML_CPP_LIBS)

submodules: cpp-netlib yaml-cpp

all: submodules metadatad

.PHONY: all submodules cpp-netlib yaml-cpp purge clean install deb rpm

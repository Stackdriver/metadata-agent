# Copyright 2018 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

PKG_VERSION=0.0.21
PKG_RELEASE=1
DOCKER_VERSION=0.2

LIBDIR=../lib
CPP_NETLIB_DIR=$(LIBDIR)/cpp-netlib
CPP_NETLIB_LIBDIR=$(CPP_NETLIB_DIR)/libs/network/src
NETWORK_URI_DIR=$(CPP_NETLIB_DIR)/deps/uri
NETWORK_URI_LIBDIR=$(NETWORK_URI_DIR)/src
YAML_CPP_DIR=$(LIBDIR)/yaml-cpp
YAML_CPP_LIBDIR=$(YAML_CPP_DIR)
PROMETHEUS_CPP_DIR=$(LIBDIR)/prometheus-cpp
PROMETHEUS_CPP_CORE_DIR=$(PROMETHEUS_CPP_DIR)/core
PROMETHEUS_CPP_CORE_LIBDIR=$(PROMETHEUS_CPP_CORE_DIR)
OPENCENSUS_CPP_DIR=$(LIBDIR)/opencensus-cpp
OPENCENSUS_CPP_OPENCENSUS_DIR=$(OPENCENSUS_CPP_DIR)/opencensus
OPENCENSUS_CPP_OPENCENSUS_LIBDIR=$(OPENCENSUS_CPP_OPENCENSUS_DIR)
OPENCENSUS_CPP_ABSL_INCLUDEDIR=$(OPENCENSUS_CPP_DIR)/abseil-src
OPENCENSUS_CPP_ABSL_LIBDIR=$(OPENCENSUS_CPP_DIR)/abseil-build/absl
SUBMODULE_DIRS=$(CPP_NETLIB_DIR) $(YAML_CPP_DIR) $(PROMETHEUS_CPP_DIR) $(OPENCENSUS_CPP_DIR)

GIT=git
GIT_VERSION=$(shell $(GIT) --version | grep -oh '[0-9]\+\.[0-9]\+\.[0-9]\+')

SED_I=/usr/bin/env sed -i
CMAKE=cmake
CPPFLAGS=\
    -DAGENT_VERSION='$(PKG_VERSION)-$(PKG_RELEASE)' \
    -DENABLE_DOCKER_METADATA \
    -I$(CPP_NETLIB_DIR) -I$(NETWORK_URI_DIR)/include -I$(YAML_CPP_DIR)/include \
    -I$(PROMETHEUS_CPP_CORE_DIR)/include -I$(OPENCENSUS_CPP_DIR) \
    -I$(OPENCENSUS_CPP_ABSL_INCLUDEDIR)
CXXFLAGS=\
    -std=c++11 -g -pthread -Wno-write-strings -Wno-deprecated
LDFLAGS=\
    -L$(CPP_NETLIB_LIBDIR) \
    -L$(NETWORK_URI_LIBDIR) \
    -L$(YAML_CPP_LIBDIR) \
    -L$(PROMETHEUS_CPP_CORE_LIBDIR) \
    -L$(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/stats \
    -L$(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/context \
    -L$(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/exporters/stats/prometheus \
    -L$(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/trace \
    -L$(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/tags \
    -L$(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/common/internal \
    -L$(OPENCENSUS_CPP_ABSL_LIBDIR)/numeric \
    -L$(OPENCENSUS_CPP_ABSL_LIBDIR)/strings \
    -L$(OPENCENSUS_CPP_ABSL_LIBDIR)/synchronization \
    -L$(OPENCENSUS_CPP_ABSL_LIBDIR)/time \
    -L$(OPENCENSUS_CPP_ABSL_LIBDIR)/debugging \
    -L$(OPENCENSUS_CPP_ABSL_LIBDIR)/base
LDLIBS=\
    -lcppnetlib-client-connections -lcppnetlib-server-parsers -lnetwork-uri \
    -lboost_program_options -lboost_system -lboost_thread -lboost_filesystem \
    -lpthread -lyajl -lssl -lcrypto -lyaml-cpp -lprometheus-cpp-core \
    -lopencensus_exporters_stats_prometheus \
    -lopencensus_exporters_stats_prometheus_utils \
    -lopencensus_stats_core -lopencensus_stats_recording \
    -lopencensus_tags -lopencensus_tags_context_util \
    -lopencensus_context \
    -lopencensus_trace_context_util \
    -lopencensus_trace_with_span \
    -lopencensus_trace \
    -lopencensus_common_random \
    -labsl_strings -labsl_synchronization -labsl_time \
    -labsl_int128 \
    -labsl_stacktrace -labsl_symbolize \
    -labsl_demangle_internal -labsl_internal_debugging_internal \
    -labsl_base -labsl_dynamic_annotations -labsl_internal_spinlock_wait \
    -labsl_internal_malloc_internal -labsl_internal_throw_delegate
SED_EXTRA=-e 's/-Wall/-Wall -Wno-deprecated/'

UNAME_S=$(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  CPPFLAGS+= -I/usr/local/include -I/usr/local/opt/openssl/include
  CXXFLAGS+= -Wno-deprecated-declarations -Wno-c++14-extensions
  LDFLAGS+= -L/usr/local/lib -L/usr/local/opt/openssl/lib
  SED_I+= ''
  SED_EXTRA+= -e \
      's/-Wall/-Wall -Wno-deprecated-declarations -Wno-c++14-extensions -Wno-unused-local-typedef/'
endif

ifneq ($(BOOST_ROOT),)
  CPPFLAGS+= -I$(BOOST_ROOT)/include
  LDFLAGS+= -L$(BOOST_ROOT)/lib64
endif

SRCS=\
     metadatad.cc \
     agent.cc \
     api_server.cc \
     store.cc \
     reporter.cc \
     configuration.cc \
     updater.cc \
     instance.cc \
     docker.cc \
     kubernetes.cc \
     resource.cc \
     oauth2.cc \
     metrics.cc \
     logging.cc \
     local_stream_http.cc \
     local_stream_delegate.cc \
     json.cc \
     time.cc \
     base64.cc \
     format.cc \
     health_checker.cc \
     environment.cc
OBJS=$(SRCS:%.cc=%.o)

CPP_NETLIB_LIBS=\
    $(CPP_NETLIB_LIBDIR)/libcppnetlib-client-connections.a \
    $(CPP_NETLIB_LIBDIR)/libcppnetlib-server-parsers.a \
    $(NETWORK_URI_LIBDIR)/libnetwork-uri.a
YAML_CPP_LIBS=\
    $(YAML_CPP_LIBDIR)/libyaml-cpp.a
PROMETHEUS_CPP_LIBS=\
    $(PROMETHEUS_CPP_CORE_LIBDIR)/libprometheus-cpp-core.a
OPENCENSUS_CPP_LIBS=\
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_base.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_internal_malloc_internal.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_internal_throw_delegate.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_internal_spinlock_wait.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_dynamic_annotations.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/debugging/libabsl_demangle_internal.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/debugging/libabsl_internal_debugging_internal.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/debugging/libabsl_stacktrace.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/debugging/libabsl_symbolize.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/numeric/libabsl_int128.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/synchronization/libabsl_synchronization.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/strings/libabsl_strings.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/time/libabsl_time.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/exporters/stats/prometheus/libopencensus_exporters_stats_prometheus.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/exporters/stats/prometheus/libopencensus_exporters_stats_prometheus_utils.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/stats/libopencensus_stats_core.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/context/libopencensus_context.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/tags/libopencensus_tags.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/tags/libopencensus_tags_context_util.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/trace/libopencensus_trace.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/trace/libopencensus_trace_with_span.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/trace/libopencensus_trace_context_util.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/context/libopencensus_context.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/common/internal/libopencensus_common_random.a
LIBS=$(CPP_NETLIB_LIBS) $(YAML_CPP_LIBS) $(PROMETHEUS_CPP_LIBS) $(OPENCENSUS_CPP_LIBS)

sbindir=/opt/stackdriver/metadata/sbin
INSTALL=/usr/bin/install
INSTALL_PROGRAM=$(INSTALL)
INSTALL_DATA=$(INSTALL) -m 644

metadatad: $(LIBS) $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

test:
	cd ../test && $(MAKE) test

$(OBJS): init-submodules

install: metadatad
	$(INSTALL) -d $(DESTDIR)$(sbindir) && \
	$(INSTALL_PROGRAM) -t $(DESTDIR)$(sbindir) $^

export DISTRO
PKG_NAME=stackdriver-metadata
PKG_MAINTAINER=Stackdriver Agents <stackdriver-agents@google.com>

DOCKER_IMAGE=us.gcr.io/container-monitoring-storage/stackdriver-metadata-agent
DOCKER_TAG=$(DOCKER_VERSION)-$(PKG_VERSION)-$(PKG_RELEASE)

PKG_TMPDIR=$$(mktemp -d --tmpdir metadata-build.XXXXXXXXXX)

DEB_PKG=$(PKG_NAME)_$(PKG_VERSION)-$(PKG_RELEASE)$(if $(DISTRO),.$(DISTRO),).deb
RPM_PKG=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_RELEASE)$(if $(DISTRO),.$(DISTRO),).x86_64.rpm

$(DEB_PKG): metadatad $(wildcard ../pkg/deb/debian/*) Makefile
	TMPDIR=$(PKG_TMPDIR) && export TMPDIR && \
	export DEB_DIR=$$TMPDIR/deb && \
	mkdir -p $$DEB_DIR/package && \
	$(MAKE) DESTDIR=$$DEB_DIR/package install && \
	$(INSTALL) -d $$DEB_DIR/package/DEBIAN && \
	touch $$DEB_DIR/package/DEBIAN/files && \
	(echo "$(PKG_NAME) ($(PKG_VERSION)-$(PKG_RELEASE)) trusty xenial wheezy jessie; urgency=low" >> $$DEB_DIR/changelog) && \
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
	TMPDIR=$(PKG_TMPDIR) && export TMPDIR && \
	export RPM_DIR=$$TMPDIR/rpm && \
	mkdir -p $$RPM_DIR/package && \
	$(MAKE) DESTDIR=$$RPM_DIR/package install && \
	(rpmbuild -bb --target=x86_64 -D"%_topdir $$RPM_DIR/rpmbuild" -D"%_rpmdir $(dir $@)" -D"%_rpmfilename $(notdir $@)" -D"%version $(PKG_VERSION)" -D"%release $(PKG_RELEASE)" --buildroot=$$RPM_DIR/package --noclean ../pkg/rpm/stackdriver-metadata.spec > $$RPM_DIR/rpmbuild.log 2>&1) && \
	$(RM) -r $$TMPDIR

rpm: $(RPM_PKG)

docker: $(DEB_PKG) ../pkg/docker/Dockerfile-$(DISTRO)
	TMPDIR=$(PKG_TMPDIR) && export TMPDIR && \
	export DOCKER_DIR=$$TMPDIR/docker && \
	mkdir -p $$DOCKER_DIR && \
	cp ../pkg/docker/Dockerfile-$(DISTRO) $$DOCKER_DIR/Dockerfile && \
	cp ../pkg/docker/entrypoint.sh $$DOCKER_DIR/ && \
	cp $(DEB_PKG) $$DOCKER_DIR/ && \
	docker build --build-arg package="$(DEB_PKG)" -t $(DOCKER_IMAGE):$(DOCKER_TAG) $$DOCKER_DIR && \
	$(RM) -r $$TMPDIR

docker-publish: docker
	gcloud docker -- push $(DOCKER_IMAGE):$(DOCKER_TAG)

clean:
	$(RM) metadatad $(OBJS)

purge: clean
	$(RM) -r init-submodules build-cpp-netlib build-yaml-cpp build-prometheus-cpp build-opencensus-cpp
	(cd .. && git submodule deinit -f $(SUBMODULE_DIRS:../%=%))

init-submodules:
	(cd .. && git submodule update --init --recursive $(SUBMODULE_DIRS:../%=%))
ifneq ($(findstring $(GIT_VERSION),2.7.0 2.7.1 2.7.2 2.7.3 2.7.4 2.7.5 2.7.6 2.8.0 2.8.1 2.8.2 2.8.3),)
	# Git submodule recursive init was broken between 2.7.0 and 2.8.4
	# (see https://marc.info/?l=git&m=145935867502974).
	# Ubuntu Xenial comes with git 2.7.4.
	@echo 'Fixing submodules for git $(GIT_VERSION)'
	cd $(CPP_NETLIB_DIR) && $(SED_I) -e 's@/src/.git/@../../../../.git/@' \
	    deps/*/.git
	cd $(NETWORK_URI_DIR) && $(SED_I) -e 's@/src/.git/@../../../../../../.git/@' \
	    deps/*/.git
	cd $(CPP_NETLIB_DIR) && $(SED_I) -e 's@/src/.git/@../../../../../../../.git/@' \
	    libs/network/doc/_ext/breathe/.git
	cd $(PROMETHEUS_CPP_DIR) && $(SED_I) -e 's@/src/.git/@../../../../.git/@' \
	    3rdparty/*/.git
endif
	touch init-submodules

$(CPP_NETLIB_DIR)/Makefile: init-submodules
	# Disable building of cpp-netlib unit tests and change the C++14
	# requirement to C++11 (some older compilers don't support C++14).
	cd $(CPP_NETLIB_DIR) && \
	$(SED_I) -e 's/unit_test_framework //' $(SED_EXTRA) CMakeLists.txt && \
	$(SED_I) -e 's/-std=c++14/-std=c++11/' $(SED_EXTRA) deps/uri/CMakeLists.txt && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DCPP-NETLIB_BUILD_TESTS=OFF -DCPP-NETLIB_BUILD_EXAMPLES=OFF \
	    -DUri_BUILD_TESTS=OFF -DUri_BUILD_DOCS=OFF

$(CPP_NETLIB_LIBS): build-cpp-netlib

build-cpp-netlib: $(CPP_NETLIB_DIR)/Makefile
	cd $(CPP_NETLIB_DIR) && \
	$(MAKE)
	touch build-cpp-netlib

$(YAML_CPP_DIR)/Makefile: init-submodules
	cd $(YAML_CPP_DIR) && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DYAML_CPP_BUILD_TOOLS=OFF

$(YAML_CPP_LIBS): build-yaml-cpp

build-yaml-cpp: $(YAML_CPP_DIR)/Makefile
	cd $(YAML_CPP_DIR) && \
	$(MAKE)
	touch build-yaml-cpp

$(PROMETHEUS_CPP_DIR)/Makefile: init-submodules
	cd $(PROMETHEUS_CPP_DIR) && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DENABLE_PUSH=OFF -DENABLE_PULL=OFF -DENABLE_COMPRESSION=OFF \
	    -DENABLE_TESTING=OFF

$(PROMETHEUS_CPP_LIBS): build-prometheus-cpp

build-prometheus-cpp: $(PROMETHEUS_CPP_DIR)/Makefile
	cd $(PROMETHEUS_CPP_DIR) && \
	$(MAKE)
	touch build-prometheus-cpp

$(OPENCENSUS_CPP_DIR)/Makefile: init-submodules
	cd $(OPENCENSUS_CPP_DIR) && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DBUILD_TESTING=OFF

$(OPENCENSUS_CPP_LIBS): build-opencensus-cpp

build-opencensus-cpp: $(OPENCENSUS_CPP_DIR)/Makefile
	cd $(OPENCENSUS_CPP_DIR) && \
	$(MAKE)
	touch build-opencensus-cpp

cpp-netlib: $(CPP_NETLIB_LIBS)
yaml-cpp: $(YAML_CPP_LIBS)
prometheus-cpp: $(PROMETHEUS_CPP_LIBS)
opencensus-cpp: $(OPENCENSUS_CPP_LIBS)

submodules: cpp-netlib yaml-cpp prometheus-cpp opencensus-cpp

all: submodules metadatad

.PHONY: all submodules cpp-netlib yaml-cpp prometheus-cpp opencensus-cpp purge clean install deb rpm

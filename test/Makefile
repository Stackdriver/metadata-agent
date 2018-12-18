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

LIBDIR=../lib
GTEST_MODULE=$(LIBDIR)/googletest
GTEST_DIR=$(LIBDIR)/googletest/googletest
GTEST_SOURCEDIR=$(GTEST_DIR)/src
GTEST_HEADERS=$(GTEST_DIR)/include/gtest/*.h \
              $(GTEST_DIR)/include/gtest/internal/*.h
GTEST_SRCS_=$(GTEST_SOURCEDIR)/*.cc $(GTEST_SOURCEDIR)/*.h $(GTEST_HEADERS)
GMOCK_DIR=$(LIBDIR)/googletest/googlemock
GMOCK_SOURCEDIR=$(GMOCK_DIR)/src
GMOCK_HEADERS=$(GMOCK_DIR)/include/gtest/*.h \
              $(GMOCK_DIR)/include/gtest/internal/*.h
GMOCK_SRCS_=$(GMOCK_SOURCEDIR)/*.cc $(GMOCK_SOURCEDIR)/*.h $(GMOCK_HEADERS)

# TODO: Factor out the common variables.
CPP_NETLIB_DIR=$(LIBDIR)/cpp-netlib
CPP_NETLIB_LIBDIR=$(CPP_NETLIB_DIR)/libs/network/src
NETWORK_URI_DIR=$(CPP_NETLIB_DIR)/deps/uri
NETWORK_URI_LIBDIR=$(NETWORK_URI_DIR)/src
YAML_CPP_DIR=$(LIBDIR)/yaml-cpp
YAML_CPP_LIBDIR=$(YAML_CPP_DIR)
OPENCENSUS_CPP_DIR=$(LIBDIR)/opencensus-cpp
OPENCENSUS_CPP_OPENCENSUS_DIR=$(OPENCENSUS_CPP_DIR)/opencensus
OPENCENSUS_CPP_OPENCENSUS_LIBDIR=$(OPENCENSUS_CPP_OPENCENSUS_DIR)
OPENCENSUS_CPP_ABSL_INCLUDEDIR=$(OPENCENSUS_CPP_DIR)/abseil-src
OPENCENSUS_CPP_ABSL_LIBDIR=$(OPENCENSUS_CPP_DIR)/abseil-build/absl
OPENCENSUS_CPP_PROMETHEUS_CPP_DIR=$(OPENCENSUS_CPP_DIR)/prometheus-src
OPENCENSUS_CPP_PROMETHEUS_CPP_CORE_DIR=$(OPENCENSUS_CPP_PROMETHEUS_CPP_DIR)/core
OPENCENSUS_CPP_PROMETHEUS_CPP_CORE_LIBDIR=$(OPENCENSUS_CPP_DIR)/prometheus-build/core

CPP_NETLIB_LIBS=\
    $(CPP_NETLIB_LIBDIR)/libcppnetlib-client-connections.a \
    $(CPP_NETLIB_LIBDIR)/libcppnetlib-server-parsers.a \
    $(NETWORK_URI_LIBDIR)/libnetwork-uri.a
YAML_CPP_LIBS=$(YAML_CPP_LIBDIR)/libyaml-cpp.a
OPENCENSUS_CPP_LIBS=\
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_base.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_internal_malloc_internal.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_internal_throw_delegate.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_internal_spinlock_wait.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/base/libabsl_dynamic_annotations.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/debugging/libabsl_stacktrace.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/debugging/libabsl_symbolize.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/numeric/libabsl_int128.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/synchronization/libabsl_synchronization.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/strings/libabsl_strings.a \
    $(OPENCENSUS_CPP_ABSL_LIBDIR)/time/libabsl_time.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/exporters/stats/stdout/libopencensus_exporters_stats_stdout.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/stats/libopencensus_stats_core.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/stats/libopencensus_stats_test_utils.a \
    $(OPENCENSUS_CPP_OPENCENSUS_LIBDIR)/tags/libopencensus_tags.a \
    $(OPENCENSUS_CPP_PROMETHEUS_CPP_CORE_LIBDIR)/libprometheus-cpp-core.a
CPPFLAGS+= \
    -isystem $(GTEST_DIR)/include -I$(GMOCK_DIR)/include \
    -I$(CPP_NETLIB_DIR) -I$(NETWORK_URI_DIR)/include -I$(YAML_CPP_DIR)/include \
    -I$(OPENCENSUS_CPP_DIR) -I$(OPENCENSUS_CPP_ABSL_INCLUDEDIR) \
    -I$(OPENCENSUS_CPP_PROMETHEUS_CPP_CORE_DIR)/include
CXXFLAGS=\
    -std=c++11 -g -pthread -Wno-write-strings -Wno-deprecated
LDFLAGS=\
    -L$(CPP_NETLIB_LIBDIR) \
    -L$(NETWORK_URI_LIBDIR) \
    -L$(YAML_CPP_LIBDIR) \
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
    -L$(OPENCENSUS_CPP_ABSL_LIBDIR)/base \
    -L$(OPENCENSUS_CPP_PROMETHEUS_CPP_CORE_LIBDIR)
LDLIBS=\
    -lcppnetlib-client-connections -lcppnetlib-server-parsers -lnetwork-uri \
    -lboost_program_options -lboost_system -lboost_thread -lboost_filesystem \
    -lboost_regex -lpthread -lyajl -lssl -lcrypto -lyaml-cpp \
    -lopencensus_exporters_stats_prometheus \
    -lopencensus_exporters_stats_prometheus_utils \
    -lopencensus_stats_core -lopencensus_stats_test_utils -lopencensus_stats_recording \
    -lopencensus_tags -lopencensus_tags_context_util \
    -lopencensus_context \
    -lopencensus_trace_context_util \
    -lopencensus_trace_with_span \
    -lopencensus_trace \
    -lopencensus_common_random \
    -lprometheus-cpp-core \
    -labsl_strings -labsl_synchronization -labsl_time \
    -labsl_int128 \
    -labsl_stacktrace -labsl_symbolize \
    -labsl_demangle_internal -labsl_internal_debugging_internal \
    -labsl_base -labsl_dynamic_annotations -labsl_internal_spinlock_wait \
    -labsl_internal_malloc_internal -labsl_internal_throw_delegate

UNAME_S=$(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  CPPFLAGS+= -I/usr/local/include -I/usr/local/opt/openssl/include
  CXXFLAGS+= -Wno-deprecated-declarations -Wno-c++14-extensions
  LDFLAGS+= -L/usr/local/lib -L/usr/local/opt/openssl/lib
endif

ifneq ($(BOOST_ROOT),)
  CPPFLAGS+= -I$(BOOST_ROOT)/include
  LDFLAGS+= -L$(BOOST_ROOT)/lib64
endif

# Where to find code under test.
SRC_DIR=../src

TEST_DIR=.
TEST_SOURCES=$(wildcard $(TEST_DIR)/*_unittest.cc)
TEST_OBJS=$(TEST_SOURCES:$(TEST_DIR)/%.cc=%.o)
TESTS=\
      api_server_unittest \
      base64_unittest \
      configuration_unittest \
      environment_unittest \
      format_unittest \
      health_checker_unittest \
      instance_unittest \
      metrics_unittest \
      json_unittest \
      kubernetes_unittest \
      logging_unittest \
      oauth2_unittest \
      reporter_unittest \
      resource_unittest \
      store_unittest \
      time_unittest \
      updater_unittest \
      util_unittest
UTIL_SOURCES=$(TEST_DIR)/fake_clock.cc $(TEST_DIR)/fake_http_server.cc
UTIL_OBJS=$(UTIL_SOURCES:$(TEST_DIR)/%.cc=%.o)

GTEST_LIB=gtest_lib.a

all: $(SRC_DIR)/build-cpp-netlib $(SRC_DIR)/build-yaml-cpp $(TESTS)

# TODO: Running the test prints "Running main() from gtest_main.cc".
# Figure out how to fix this.
test: $(TESTS)
	@for t in $(TESTS); do \
		echo "Running $$t"; \
		./$$t || exit 1; \
	done

clean:
	$(RM) $(TESTS) $(GTEST_LIB) $(TEST_OBJS) $(UTIL_OBJS)

purge: clean
	$(RM) init-submodules
	(cd .. && git submodule deinit -f $(GTEST_MODULE:../%=%))

init-submodules:
	(cd .. && git submodule update --init $(GTEST_MODULE:../%=%))
	touch init-submodules

$(SRC_DIR)/init-submodules:
	cd $(SRC_DIR) && $(MAKE) $(@:$(SRC_DIR)/%=%)

$(SRC_DIR)/build-cpp-netlib $(SRC_DIR)/build-yaml-cpp $(SRC_DIR)/build-opencensus-cpp: $(SRC_DIR)/init-submodules
	cd $(SRC_DIR) && $(MAKE) $(@:$(SRC_DIR)/%=%)

$(CPP_NETLIB_LIBS): $(SRC_DIR)/build-cpp-netlib

$(YAML_CPP_LIBS): $(SRC_DIR)/build-yaml-cpp

$(OPENCENSUS_CPP_LIBS): $(SRC_DIR)/build-opencensus-cpp

$(SRC_DIR)/%.o: $(SRC_DIR)/build-cpp-netlib $(SRC_DIR)/build-yaml-cpp $(SRC_DIR)/build-opencensus-cpp $(SRC_DIR)/%.cc
	cd $(SRC_DIR) && $(MAKE) $(@:$(SRC_DIR)/%=%)

$(GTEST_SOURCEDIR)/gtest-all.cc: init-submodules
$(GMOCK_SOURCEDIR)/gmock-all.cc $(GMOCK_SOURCEDIR)/gmock_main.cc: init-submodules

gtest-all.o: $(GTEST_SOURCEDIR)/gtest-all.cc
	$(CXX) -c $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -o $@ $^
gmock-all.o: $(GMOCK_SOURCEDIR)/gmock-all.cc
	$(CXX) -c $(CPPFLAGS) -I$(GMOCK_DIR) $(CXXFLAGS) -o $@ $^

gmock_main.o: $(GMOCK_SOURCEDIR)/gmock_main.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $^

$(GTEST_LIB): gtest-all.o gmock-all.o gmock_main.o
	$(AR) $(ARFLAGS) $@ $^

$(TESTS): $(GTEST_LIB) $(CPP_NETLIB_LIBS) $(YAML_CPP_LIBS) $(OPENCENSUS_CPP_LIBS)

# All unittest objects depend on GTEST_LIB.
# Some headers need CPP_NETLIB_LIBS and YAML_CPP_LIBS.
$(TESTS:%=%.o): $(GTEST_LIB) $(CPP_NETLIB_LIBS) $(YAML_CPP_LIBS) $(OPENCENSUS_CPP_LIBS)

api_server_unittest: api_server_unittest.o $(SRC_DIR)/api_server.o $(SRC_DIR)/metrics.o $(SRC_DIR)/configuration.o $(SRC_DIR)/store.o $(SRC_DIR)/json.o $(SRC_DIR)/resource.o $(SRC_DIR)/logging.o $(SRC_DIR)/time.o $(SRC_DIR)/health_checker.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
base64_unittest: base64_unittest.o $(SRC_DIR)/base64.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
configuration_unittest: configuration_unittest.o $(SRC_DIR)/configuration.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
environment_unittest: environment_unittest.o fake_http_server.o $(SRC_DIR)/environment.o $(SRC_DIR)/configuration.o $(SRC_DIR)/format.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
format_unittest: format_unittest.o $(SRC_DIR)/format.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
health_checker_unittest: health_checker_unittest.o $(SRC_DIR)/health_checker.o $(SRC_DIR)/configuration.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/resource.o $(SRC_DIR)/store.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -lboost_filesystem -lboost_system -o $@
instance_unittest: instance_unittest.o $(SRC_DIR)/instance.o $(SRC_DIR)/configuration.o $(SRC_DIR)/environment.o $(SRC_DIR)/format.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/resource.o $(SRC_DIR)/store.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
json_unittest: json_unittest.o $(SRC_DIR)/json.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
kubernetes_unittest: kubernetes_unittest.o fake_clock.o fake_http_server.o $(SRC_DIR)/kubernetes.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/environment.o $(SRC_DIR)/time.o $(SRC_DIR)/store.o $(SRC_DIR)/resource.o $(SRC_DIR)/updater.o $(SRC_DIR)/instance.o $(SRC_DIR)/format.o $(SRC_DIR)/configuration.o $(SRC_DIR)/health_checker.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
logging_unittest: logging_unittest.o $(SRC_DIR)/logging.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
metrics_unittest: metrics_unittest.o $(SRC_DIR)/metrics.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
oauth2_unittest: oauth2_unittest.o fake_clock.o fake_http_server.o $(SRC_DIR)/oauth2.o $(SRC_DIR)/base64.o $(SRC_DIR)/configuration.o $(SRC_DIR)/environment.o $(SRC_DIR)/format.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/metrics.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
resource_unittest: resource_unittest.o $(SRC_DIR)/resource.o $(SRC_DIR)/json.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
reporter_unittest: reporter_unittest.o fake_clock.o fake_http_server.o $(SRC_DIR)/reporter.o $(SRC_DIR)/base64.o $(SRC_DIR)/configuration.o $(SRC_DIR)/environment.o $(SRC_DIR)/format.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/metrics.o $(SRC_DIR)/oauth2.o $(SRC_DIR)/resource.o $(SRC_DIR)/store.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
store_unittest: store_unittest.o $(SRC_DIR)/store.o $(SRC_DIR)/resource.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/time.o $(SRC_DIR)/configuration.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
time_unittest: time_unittest.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
updater_unittest: updater_unittest.o fake_clock.o $(SRC_DIR)/updater.o $(SRC_DIR)/resource.o $(SRC_DIR)/store.o $(SRC_DIR)/configuration.o $(SRC_DIR)/logging.o $(SRC_DIR)/time.o $(SRC_DIR)/json.o $(SRC_DIR)/format.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
util_unittest: util_unittest.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: all test clean purge

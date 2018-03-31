LIBDIR=../lib
GTEST_MODULE=$(LIBDIR)/googletest
GTEST_DIR=$(LIBDIR)/googletest/googletest
GTEST_SOURCEDIR=$(GTEST_DIR)/src
GTEST_HEADERS=$(GTEST_DIR)/include/gtest/*.h \
              $(GTEST_DIR)/include/gtest/internal/*.h
GTEST_SRCS_=$(GTEST_SOURCEDIR)/*.cc $(GTEST_SOURCEDIR)/*.h $(GTEST_HEADERS)
GMOCK_DIR=$(LIBDIR)/googletest/googlemock

# TODO: Factor out the common variables.
CPP_NETLIB_DIR=$(LIBDIR)/cpp-netlib
CPP_NETLIB_LIBDIR=$(CPP_NETLIB_DIR)/libs/network/src
NETWORK_URI_DIR=$(CPP_NETLIB_DIR)/deps/uri
NETWORK_URI_LIBDIR=$(NETWORK_URI_DIR)/src
YAML_CPP_DIR=$(LIBDIR)/yaml-cpp
YAML_CPP_LIBDIR=$(YAML_CPP_DIR)

CPP_NETLIB_LIBS=\
    $(CPP_NETLIB_LIBDIR)/libcppnetlib-client-connections.a \
    $(CPP_NETLIB_LIBDIR)/libcppnetlib-server-parsers.a \
    $(NETWORK_URI_LIBDIR)/libnetwork-uri.a
YAML_CPP_LIBS=$(YAML_CPP_LIBDIR)/libyaml-cpp.a

CPPFLAGS+= \
    -DENABLE_KUBERNETES_METADATA \
    -isystem $(GTEST_DIR)/include -I$(GMOCK_DIR)/include \
    -I$(CPP_NETLIB_DIR) -I$(NETWORK_URI_DIR)/include -I$(YAML_CPP_DIR)/include
CXXFLAGS=\
    -std=c++11 -g -pthread -Wno-write-strings -Wno-deprecated
LDFLAGS=-L$(CPP_NETLIB_LIBDIR) -L$(NETWORK_URI_LIBDIR) -L$(YAML_CPP_LIBDIR)
LDLIBS=\
    -lcppnetlib-client-connections -lcppnetlib-server-parsers -lnetwork-uri \
    -lboost_program_options -lboost_system -lboost_thread -lboost_filesystem \
    -lpthread -lyajl -lssl -lcrypto -lyaml-cpp

# Where to find code under test.
SRC_DIR=../src

TEST_DIR=.
TEST_SOURCES=$(wildcard $(TEST_DIR)/*_unittest.cc)
TEST_OBJS=$(TEST_SOURCES:$(TEST_DIR)/%.cc=%.o)
TESTS=\
      api_server_unittest \
      base64_unittest \
      configuration_unittest \
      format_unittest \
      health_checker_unittest \
      instance_unittest \
      json_unittest \
      kubernetes_unittest \
      resource_unittest \
      store_unittest \
      time_unittest \
      updater_unittest

GTEST_LIB=gtest_lib.a

all: $(TESTS)

# TODO: Running the test prints "Running main() from gtest_main.cc".
# Figure out how to fix this.
test: $(TESTS)
	@for t in $(TESTS); do \
		echo "Running $$t"; \
		./$$t; \
	done

clean:
	$(RM) $(TESTS) $(GTEST_LIB) $(TEST_OBJS)

purge: clean
	$(RM) init-submodules
	git submodule deinit -f $(GTEST_MODULE)

init-submodules:
	git submodule update --init $(GTEST_MODULE)
	touch init-submodules

$(CPP_NETLIB_LIBS):
	cd $(SRC_DIR) && $(MAKE) $@

$(YAML_CPP_LIBS):
	cd $(SRC_DIR) && $(MAKE) $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cc
	cd $(SRC_DIR) && $(MAKE) $(@:$(SRC_DIR)/%=%)

$(GTEST_SOURCEDIR)/gtest-all.cc $(GTEST_SOURCEDIR)/gtest_main.cc: init-submodules

gtest-all.o: $(GTEST_SOURCEDIR)/gtest-all.cc
	$(CXX) -c $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -o $@ $^

gtest_main.o: $(GTEST_SOURCEDIR)/gtest_main.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $^

$(GTEST_LIB): gtest-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

$(TESTS): $(GTEST_LIB) $(CPP_NETLIB_LIBS) $(YAML_CPP_LIBS)

# All unittest objects depend on GTEST_LIB.
# Some headers need CPP_NETLIB_LIBS and YAML_CPP_LIBS.
$(TESTS:%=%.o): $(GTEST_LIB) $(CPP_NETLIB_LIBS) $(YAML_CPP_LIBS)

api_server_unittest: api_server_unittest.o $(SRC_DIR)/api_server.o $(SRC_DIR)/configuration.o $(SRC_DIR)/store.o $(SRC_DIR)/json.o $(SRC_DIR)/resource.o $(SRC_DIR)/logging.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
format_unittest: format_unittest.o $(SRC_DIR)/format.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
base64_unittest: base64_unittest.o $(SRC_DIR)/base64.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
configuration_unittest: configuration_unittest.o $(SRC_DIR)/configuration.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
health_checker_unittest: health_checker_unittest.o $(SRC_DIR)/health_checker.o $(SRC_DIR)/configuration.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -lboost_filesystem -lboost_system -o $@
instance_unittest: instance_unittest.o $(SRC_DIR)/instance.o $(SRC_DIR)/configuration.o $(SRC_DIR)/environment.o $(SRC_DIR)/format.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/resource.o $(SRC_DIR)/store.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
json_unittest: json_unittest.o $(SRC_DIR)/json.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
kubernetes_unittest: kubernetes_unittest.o $(SRC_DIR)/kubernetes.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/environment.o $(SRC_DIR)/time.o $(SRC_DIR)/store.o $(SRC_DIR)/resource.o $(SRC_DIR)/updater.o $(SRC_DIR)/instance.o $(SRC_DIR)/format.o $(SRC_DIR)/configuration.o $(SRC_DIR)/health_checker.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
resource_unittest: resource_unittest.o $(SRC_DIR)/resource.o $(SRC_DIR)/json.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
store_unittest: store_unittest.o $(SRC_DIR)/store.o $(SRC_DIR)/resource.o $(SRC_DIR)/json.o $(SRC_DIR)/logging.o $(SRC_DIR)/time.o $(SRC_DIR)/configuration.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
time_unittest: time_unittest.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
updater_unittest: updater_unittest.o $(SRC_DIR)/updater.o $(SRC_DIR)/resource.o $(SRC_DIR)/store.o $(SRC_DIR)/configuration.o $(SRC_DIR)/logging.o $(SRC_DIR)/time.o $(SRC_DIR)/json.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: all test clean purge

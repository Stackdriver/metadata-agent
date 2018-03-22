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
YAML_CPP_DIR=$(LIBDIR)/yaml-cpp
YAML_CPP_LIBDIR=$(YAML_CPP_DIR)

YAML_CPP_LIBS=$(YAML_CPP_LIBDIR)/libyaml-cpp.a

CPPFLAGS+= \
    -isystem $(GTEST_DIR)/include -I$(GMOCK_DIR)/include \
    -I$(YAML_CPP_DIR)/include
CXXFLAGS=-std=c++11 -g -pthread
LDLIBS=-lpthread -lboost_program_options -lyaml-cpp -lyajl
LDFLAGS=-L$(YAML_CPP_LIBDIR)

# Where to find code under test.
SRC_DIR=../src

TEST_DIR=.
TEST_SOURCES=$(wildcard $(TEST_DIR)/*_unittest.cc)
TEST_OBJS=$(TEST_SOURCES:$(TEST_DIR)/%.cc=%.o)
TESTS=\
      base64_unittest \
      configuration_unittest \
      format_unittest \
      resource_unittest \
      time_unittest

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

format_unittest: $(GTEST_LIB) format_unittest.o $(SRC_DIR)/format.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
base64_unittest: $(GTEST_LIB) base64_unittest.o $(SRC_DIR)/base64.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
configuration_unittest: $(GTEST_LIB) $(YAML_CPP_LIBS) configuration_unittest.o $(SRC_DIR)/configuration.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
resource_unittest: $(GTEST_LIB) resource_unittest.o $(SRC_DIR)/resource.o $(SRC_DIR)/json.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
time_unittest: $(GTEST_LIB) time_unittest.o $(SRC_DIR)/time.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: all test clean purge

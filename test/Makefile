LIBDIR=../lib
GTEST_MODULE=$(LIBDIR)/googletest
GTEST_DIR=$(LIBDIR)/googletest/googletest
GTEST_SOURCEDIR=$(GTEST_DIR)/src
GTEST_HEADERS=$(GTEST_DIR)/include/gtest/*.h \
              $(GTEST_DIR)/include/gtest/internal/*.h
GTEST_SRCS_=$(GTEST_SOURCEDIR)/*.cc $(GTEST_SOURCEDIR)/*.h $(GTEST_HEADERS)

# TODO: Factor out the common variables.
CPP_NETLIB_DIR=$(LIBDIR)/cpp-netlib
YAML_CPP_DIR=$(LIBDIR)/yaml-cpp

CPPFLAGS+=-isystem $(GTEST_DIR)/include
CXXFLAGS=-std=c++11 -g -pthread

# Where to find code under test.
SRC_DIR=../src

TEST_DIR=.
TEST_SOURCES=$(wildcard $(TEST_DIR)/*_unittest.cc)
TEST_OBJS=$(TEST_SOURCES:%.cc=%.o)
TESTS=\
      format_unittest

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


$(SRC_DIR)/%.o: $(SRC_DIR)/%.cc
	cd $(SRC_DIR) && $(MAKE) $(@:$(SRC_DIR)/%=%)

$(GTEST_SOURCEDIR)/gtest-all.cc $(GTEST_SOURCEDIR)/gtest_main.cc: init-submodules

gtest-all.o: $(GTEST_SOURCEDIR)/gtest-all.cc
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c -o $@ $^

gtest_main.o: $(GTEST_SOURCEDIR)/gtest_main.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $^

$(GTEST_LIB): gtest-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

format_unittest: $(GTEST_LIB) format_unittest.o $(SRC_DIR)/format.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -lpthread -o $@

.PHONY: all test clean purge

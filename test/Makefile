LIBDIR=../lib
GTEST_MODULE=$(LIBDIR)/googletest
GTEST_DIR=$(LIBDIR)/googletest/googletest
GTEST_SOURCEDIR=$(GTEST_DIR)/src
GTEST_HEADERS=$(GTEST_DIR)/include/gtest/*.h \
              $(GTEST_DIR)/include/gtest/internal/*.h
GTEST_SRCS_=$(GTEST_SOURCEDIR)/*.cc $(GTEST_SOURCEDIR)/*.h $(GTEST_HEADERS)

CPP_NETLIB_DIR=$(LIBDIR)/cpp-netlib
YAML_CPP_DIR=$(LIBDIR)/yaml-cpp

CPPFLAGS+=-isystem $(GTEST_DIR)/include
CXXFLAGS=-std=c++11 -g -pthread

# Where to find code under test.
SRC_DIR=../src
SRCS=\
     $(SRC_DIR)/format.cc
OBJS=$(subst src/,test/,$(subst .cc,.o,$(SRCS)))

TEST_DIR=.
TESTS=$(wildcard $(TEST_DIR)/*.cc)
TEST_OBJS=$(TESTS:%.cc=%.o)

GTEST_LIBS=gtest_main.a

all: build-googletest unittest

clean:
	rm -f unittest *.a *.o

purge: clean
	git submodule deinit -f $(GTEST_MODULE)

init-submodules:
	git submodule update --init $(GTEST_MODULE)

$(GTEST_SRCS_): init-submodules
$(GTEST_DIR)/Makefile: $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
	    $(GTEST_SOURCEDIR)/gtest-all.cc
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
	    $(GTEST_SOURCEDIR)/gtest_main.cc

$(GTEST_LIBS): build-googletest

build-googletest: $(GTEST_DIR)/Makefile
	$(AR) $(ARFLAGS) gtest_main.a gtest-all.o gtest_main.o

build-source: $(SRCS) $(GTEST_HEADERS)
	$(CXX) $(CPPFLAGS) -I$(CPP_NETLIB_DIR) \
	    -I$(YAML_CPP_DIR)/include $(CXXFLAGS) -c $(SRCS)

build-test: $(TESTS) $(GTEST_HEADERS)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $(TESTS)

$(OBJS): build-source
$(TEST_OBJS): build-test
unittest: $(GTEST_LIBS) $(OBJS) $(TEST_OBJS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -lpthread $^ -o $@

.PHONY: all clean purge

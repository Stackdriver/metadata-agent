LIBDIR=../lib
CPP_NETLIB_DIR=$(LIBDIR)/cpp-netlib
CPP_NETLIB_LIBDIR=$(CPP_NETLIB_DIR)/libs/network/src
YAML_CPP_DIR=$(LIBDIR)/yaml-cpp
YAML_CPP_LIBDIR=$(YAML_CPP_DIR)

SED=/usr/bin/env sed
CMAKE=cmake
CXXFLAGS=-std=c++11 -g -I$(CPP_NETLIB_DIR) -I$(YAML_CPP_DIR)/include
LDFLAGS=-L$(CPP_NETLIB_LIBDIR) -L$(YAML_CPP_LIBDIR)
LDLIBS=-lcppnetlib-uri -lcppnetlib-client-connections -lboost_system \
    -lboost_thread -lpthread -lyajl -lssl -lcrypto -lyaml-cpp

SRCS=\
     metadatad.cc \
     api_server.cc \
     configuration.cc \
     updater.cc \
     docker.cc \
     resource.cc \
     oauth2.cc \
     logging.cc \
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

metadatad: $(LIBS) $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

$(OBJS): init-submodules

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

.PHONY: all submodules cpp-netlib yaml-cpp purge clean

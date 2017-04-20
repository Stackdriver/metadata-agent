SED=/usr/bin/env sed
CMAKE=cmake
CXXFLAGS=-std=c++11 -g -I../lib/cpp-netlib -I../lib/yaml-cpp/include
LDFLAGS=-L../lib/cpp-netlib/libs/network/src -L../lib/yaml-cpp
LDLIBS=-lcppnetlib-uri -lcppnetlib-client-connections -lboost_system -lboost_thread -lpthread -lyajl -lssl -lcrypto -lyaml-cpp

SRCS=\
     metadatad.cc \
     api_server.cc \
     configuration.cc \
     updater.cc \
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
    ../lib/cpp-netlib/libs/network/src/libcppnetlib-uri.a \
    ../lib/cpp-netlib/libs/network/src/libcppnetlib-client-connections.a
YAML_CPP_LIBS=\
    ../lib/yaml-cpp/libyaml-cpp.a
LIBS=$(CPP_NETLIB_LIBS) $(YAML_CPP_LIBS)

metadatad: $(LIBS) $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

clean:
	$(RM) metadatad $(OBJS)

purge:
	$(RM) -r ../lib/cpp-netlib ../lib/yaml-cpp

../lib/cpp-netlib/CMakeLists.txt ../lib/yaml-cpp/CMakeLists.txt:
	cd .. && \
	git submodule init && \
	git submodule update

init-submodules: ../lib/cpp-netlib/CMakeLists.txt ../lib/yaml-cpp/CMakeLists.txt

../lib/cpp-netlib/Makefile: ../lib/cpp-netlib/CMakeLists.txt
	cd ../lib/cpp-netlib && \
	$(SED) -i -e 's/unit_test_framework //' CMakeLists.txt && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DCPP-NETLIB_BUILD_TESTS=OFF -DCPP-NETLIB_BUILD_EXAMPLES=OFF

$(CPP_NETLIB_LIBS): ../lib/cpp-netlib/Makefile
	cd ../lib/cpp-netlib && \
	$(MAKE) $(MAKEFLAGS)

../lib/yaml-cpp/Makefile: ../lib/yaml-cpp/CMakeLists.txt
	cd ../lib/yaml-cpp && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DYAML_CPP_BUILD_TOOLS=OFF

$(YAML_CPP_LIBS): ../lib/yaml-cpp/Makefile
	cd ../lib/yaml-cpp && \
	$(MAKE) $(MAKEFLAGS)

cpp-netlib: $(CPP_NETLIB_LIBS)

yaml-cpp: $(YAML_CPP_LIBS)

submodules: cpp-netlib yaml-cpp

all: submodules metadatad

.PHONY: all submodules cpp-netlib yaml-cpp init-submodules purge clean

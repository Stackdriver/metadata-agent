SED=/usr/bin/sed
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

metadatad: $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

clean:
	$(RM) metadatad $(OBJS)

init-submodules:
	cd .. && \
	git submodule init && \
	git submodule update

cpp-netlib: init-submodules
	cd ../lib/cpp-netlib && \
	$(SED) -i -e 's/unit_test_framework //' CMakeLists.txt && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DCPP-NETLIB_BUILD_TESTS=OFF -DCPP-NETLIB_BUILD_EXAMPLES=OFF && \
	$(MAKE) $(MAKEFLAGS)

yaml-cpp: init-submodules
	cd ../lib/yaml-cpp && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DYAML_CPP_BUILD_TOOLS=OFF && \
	$(MAKE) $(MAKEFLAGS)

submodules: cpp-netlib yaml-cpp

all: submodules metadatad

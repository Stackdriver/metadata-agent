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
     format.cc
OBJS=$(SRCS:%.cc=%.o)

metadatad: $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

clean:
	$(RM) metadatad $(OBJS)


CXXFLAGS=-std=c++11 -g
LDLIBS=-lcppnetlib-uri -lcppnetlib-client-connections -lboost_system -lboost_thread -lpthread -lyajl -lcrypto

SRCS=metadatad.cc api_server.cc updater.cc resource.cc oauth2.o logging.cc local_stream_http.cc local_stream_delegate.cc json.cc
OBJS=$(SRCS:%.cc=%.o)

metadatad: $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

clean:
	$(RM) metadatad $(OBJS)


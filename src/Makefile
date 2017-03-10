CXXFLAGS=-std=c++11 -g
LDLIBS=-lcppnetlib-uri -lboost_system -lboost_thread -lpthread

SRCS=metadatad.cc api_server.cc updater.cc resource.cc logging.cc local_stream_http.cc local_stream_delegate.cc
OBJS=$(SRCS:%.cc=%.o)

metadatad: $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

clean:
	$(RM) metadatad $(OBJS)


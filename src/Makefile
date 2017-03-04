CXXFLAGS=-std=c++11
LDLIBS=-lboost_system -lpthread

SRCS=metadatad.cc api_server.cc updater.cc resource.cc logging.cc
OBJS=$(SRCS:%.cc=%.o)

metadatad: $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

clean:
	$(RM) metadatad $(OBJS)


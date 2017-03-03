CXXFLAGS=-std=c++11
LDLIBS=-lboost_system -lpthread

SRCS=metadatad.c api_server.c updater.c resource.c
OBJS=$(SRCS:%.c=%.o)

metadatad: $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

clean:
	$(RM) metadatad $(OBJS)


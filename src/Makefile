CXXFLAGS=-std=c++11 -g
LDLIBS=-lyajl

SRCS=\
     metadatad.cc \
     resource.cc \
     logging.cc \
     json.cc \
     time.cc
OBJS=$(SRCS:%.cc=%.o)

LIBS=

metadatad: $(LIBS) $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

clean:
	$(RM) metadatad $(OBJS)

all: metadatad

.PHONY: all clean

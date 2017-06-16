CXXFLAGS=-std=c++11 -g
LDLIBS=

SRCS=\
     metadatad.cc
OBJS=$(SRCS:%.cc=%.o)

LIBS=

metadatad: $(LIBS) $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

clean:
	$(RM) metadatad $(OBJS)

all: metadatad

.PHONY: all clean

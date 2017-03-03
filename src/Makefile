CXXFLAGS=-std=c++11
LDLIBS=-lboost_system -lpthread

metadatad: metadatad.o api_server.o updater.o resource.o
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)


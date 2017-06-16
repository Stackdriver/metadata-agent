LIBDIR=../lib
YAML_CPP_DIR=$(LIBDIR)/yaml-cpp
YAML_CPP_LIBDIR=$(YAML_CPP_DIR)

CMAKE=cmake
CXXFLAGS=-std=c++11 -g -I$(YAML_CPP_DIR)/include
LDFLAGS=-L$(YAML_CPP_LIBDIR)
LDLIBS=-lyajl -lyaml-cpp

SRCS=\
     metadatad.cc \
     configuration.cc \
     resource.cc \
     logging.cc \
     json.cc \
     time.cc
OBJS=$(SRCS:%.cc=%.o)

YAML_CPP_LIBS=\
    $(YAML_CPP_LIBDIR)/libyaml-cpp.a
LIBS=$(YAML_CPP_LIBS)

metadatad: $(LIBS) $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LDLIBS)

$(OBJS): init-submodules

clean:
	$(RM) metadatad $(OBJS)

purge: clean
	$(RM) -r init-submodules build-yaml-cpp \
	    $(YAML_CPP_DIR)
	git submodule deinit --all

init-submodules:
	cd .. && \
	git submodule init && \
	git submodule update
	touch init-submodules

$(YAML_CPP_DIR)/Makefile: init-submodules
	cd $(YAML_CPP_DIR) && \
	$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
	    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
	    -DYAML_CPP_BUILD_TOOLS=OFF

$(YAML_CPP_LIBS): build-yaml-cpp

build-yaml-cpp: $(YAML_CPP_DIR)/Makefile
	cd $(YAML_CPP_DIR) && \
	$(MAKE) $(MAKEFLAGS)
	touch build-yaml-cpp

yaml-cpp: $(YAML_CPP_LIBS)

submodules: cpp-netlib yaml-cpp

all: submodules metadatad

.PHONY: all submodules yaml-cpp purge clean

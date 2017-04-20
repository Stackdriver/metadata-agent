This is the Stackdriver metadata agent.

# Prerequisites

1. Install runtime dependencies:

       $ sudo apt-get install libyajl2 libboost-system1.55.0 libboost-thread1.55.0

2. Install build dependencies:

       $ sudo apt-get install cmake libyajl-dev libssl-dev libboost1.55-dev \
         libboost-system1.55-dev libboost-atomic1.55-dev libboost-chrono1.55-dev \
         libboost-date-time1.55-dev libboost-filesystem1.55-dev \
         libboost-program-options1.55-dev libboost-regex1.55-dev \
         libboost-thread1.55-dev libboost-timer1.55-dev

## Ubuntu 16.04 special edition

1. Install runtime dependencies (Ubuntu 16.04 special edition):

       $ sudo apt-get install libyajl2 libboost-system1.58.0 libboost-thread1.58.0

2. Install build dependencies (Ubuntu 16.04 special edition):

       $ sudo apt-get install cmake libyajl-dev libssl-dev libboost1.58-dev \
         libboost-system1.58-dev libboost-atomic1.58-dev libboost-chrono1.58-dev \
         libboost-date-time1.58-dev libboost-filesystem1.58-dev \
         libboost-program-options1.58-dev libboost-regex1.58-dev \
         libboost-thread1.58-dev libboost-timer1.58-dev

# Building

<!-- Old stuff, not needed anymore.
1. Init the submodules:

       $ git submodule init
       $ git submodule update

2. Build `cpp-netlib`:

       $ cd lib/cpp-netlib
       $ sed -i -e 's/unit_test_framework //' CMakeLists.txt
       $ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
         -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
         -DCPP-NETLIB_BUILD_TESTS=OFF -DCPP-NETLIB_BUILD_EXAMPLES=OFF
       $ make -j10
       $ cd -

3. Build `yaml-cpp`:

       $ cd lib/yaml-cpp
       $ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 \
         -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
         -DYAML_CPP_BUILD_TOOLS=OFF
       $ make -j10
       $ cd -
-->

1. Build the metadata agent:

       $ cd src
       $ make -j10

# Running

1. Run the agent with default settings:

       $ cd src
       $ ./metadatad

   The agent will use the default credentials locations or the metadata server.

2. Run the agent with modified configuration:

       $ cd src
       $ ./metadatad sample_agent_config.yaml

   The default credentials location in the sample configuration is `/tmp/token.json`.

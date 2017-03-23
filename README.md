This is the Stackdriver metadata agent.

# Building

1. Init the submodules:

       $ git submodule init
       $ git submodule update

2. Build `cpp-netlib`:

       $ cd lib/cpp-netlib
       $ sed -i -e 's/unit_test_framework //' CMakeLists.txt
       $ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCPP-NETLIB_BUILD_TESTS=OFF -DCPP-NETLIB_BUILD_EXAMPLES=OFF
       $ make -j10
       $ cd -

3. Build `yaml-cpp`:

       $ cd lib/yaml-cpp
       $ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-std=c++11 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DYAML_CPP_BUILD_TOOLS=OFF
       $ make -j10
       $ cd -

4. Build the metadata agent:

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

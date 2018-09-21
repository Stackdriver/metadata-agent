This is the Stackdriver metadata agent.

# Prerequisites

## Ubuntu 14.04 (trusty)

1. Install runtime dependencies:

       $ sudo apt-get install libboost-filesystem1.55.0 \
         libboost-program-options1.55.0 libboost-system1.55.0 \
         libboost-thread1.55.0 libyajl2

2. Install build dependencies:

       $ sudo apt-get install cmake dpkg-dev g++ libboost-atomic1.55-dev \
         libboost-chrono1.55-dev libboost-date-time1.55-dev \
         libboost-filesystem1.55-dev libboost-program-options1.55-dev \
         libboost-regex1.55-dev libboost-system1.55-dev \
         libboost-thread1.55-dev libboost-timer1.55-dev libboost1.55-dev \
         libssl-dev libyajl-dev

## Ubuntu 16.04 (xenial)

1. Install runtime dependencies:

       $ sudo apt-get install ca-certificates libboost-filesystem1.58.0 \
         libboost-program-options1.58.0 libboost-system1.58.0 \
         libboost-thread1.58.0 libssl1.0.0 libyajl2

2. Install build dependencies:

       $ sudo apt-get install cmake dpkg-dev g++ libboost-atomic1.58-dev \
         libboost-chrono1.58-dev libboost-date-time1.58-dev \
         libboost-filesystem1.58-dev libboost-program-options1.58-dev \
         libboost-regex1.58-dev libboost-system1.58-dev \
         libboost-thread1.58-dev libboost-timer1.58-dev libboost1.58-dev \
         libssl-dev libyajl-dev

## Debian 9 (stretch)

1. Install runtime dependencies:

       $ sudo apt-get install ca-certificates libboost-filesystem1.62.0 \
         libboost-program-options1.62.0 libboost-system1.62.0 \
         libboost-thread1.62.0 libssl1.0.2 libyajl2

2. Install build dependencies:

       $ sudo apt-get install cmake dpkg-dev g++ libboost-atomic1.62-dev \
         libboost-chrono1.62-dev libboost-date-time1.62-dev \
         libboost-filesystem1.62-dev libboost-program-options1.62-dev \
         libboost-regex1.62-dev libboost-system1.62-dev \
         libboost-thread1.62-dev libboost-timer1.62-dev libboost1.62-dev \
         libssl1.0-dev libyajl-dev

## CentOS 7

1. Prepare external boost repo:

       $ sudo tee /etc/yum.repos.d/puias-computational-x86_64.repo << EOM
       [puias-computational-x86_64]
       name=PUIAS Computational x86_64
       baseurl=http://springdale.math.ias.edu/data/puias/computational/7/x86_64
       enabled=1
       gpgcheck=1
       EOM
       $ sudo rpm --import http://springdale.math.ias.edu/data/puias/7/x86_64/os/RPM-GPG-KEY-puias

2. Install runtime dependencies:

       $ sudo yum install -y boost155-filesystem boost155-program-options \
         boost155-system boost155-thread yajl

3. Install build dependencies:

       $ sudo yum install -y boost155-devel cmake gcc-c++ make openssl-devel \
         rpm-build yajl-devel

4. Set up Boost root:

       $ sudo tee /etc/profile.d/boost.sh << EOM
       export BOOST_ROOT=/usr/local/boost/1.55.0
       export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$BOOST_ROOT/lib64"
       EOM
       $ . /etc/profile.d/boost.sh

## MacOS 10.12

1. Install runtime dependencies:

   *Note: this currently does not work with boost@1.67, which is the latest
   available from Homebrew as of 2018/05/17*.

       $ brew install openssl
       $ brew install boost\@1.60 -c++11 && \
         (cd /usr/local/lib && ln -s libboost_thread-mt.a libboost_thread.a && \
          ln -s libboost_thread-mt.dylib libboost_thread.dylib)
       $ brew install yajl

2. Install build dependencies:

       $ brew install cmake

# Building

1. Build the metadata agent:

       $ cd src
       $ make -j10

# Testing

1. Run all tests:

       $ cd src
       $ make test

2. Run individual tests:

       $ cd test
       $ make <test_name>
       $ ./<test_name>

# Packaging

1. Build the DEB package:

       $ cd src
       $ make deb

   If you want to embed the distro name (e.g., "xenial") into the package
   filename, use:

       $ cd src
       $ make DISTRO=xenial deb

2. Build the RPM package:

       $ cd src
       $ make rpm

   If you want to embed the distro name (e.g., "el7") into the package
   filename, use:

       $ cd src
       $ make DISTRO=el7 rpm

# Running

1. Run the agent with default settings:

       $ cd src
       $ ./metadatad

   The agent will use the default credentials locations or the metadata server.

2. Run the agent with modified configuration:

       $ cd src
       $ ./metadatad sample_agent_config.yaml

   The default credentials location in the sample configuration is `/tmp/token.json`.

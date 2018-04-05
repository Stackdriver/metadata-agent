This is the Stackdriver metadata agent.

# Prerequisites

<!---
## Ubuntu 14.04 (trusty)

1. Install runtime dependencies:

       $ sudo apt-get install libyajl2 libboost-filesystem1.55.0 \
         libboost-program-options1.55.0 libboost-system1.55.0 \
         libboost-thread1.55.0

2. Install build dependencies:

       $ sudo apt-get install g++ cmake dpkg-dev libyajl-dev libssl-dev \
         libboost1.55-dev libboost-system1.55-dev libboost-atomic1.55-dev \
         libboost-chrono1.55-dev libboost-date-time1.55-dev \
         libboost-filesystem1.55-dev libboost-program-options1.55-dev \
         libboost-regex1.55-dev libboost-thread1.55-dev libboost-timer1.55-dev

-->
## Ubuntu 16.04 (xenial)

1. Install runtime dependencies:

       $ sudo apt-get install libssl1.0.0 libyajl2 libboost-filesystem1.58.0 \
         libboost-program-options1.58.0 libboost-system1.58.0 \
         libboost-thread1.58.0 ca-certificates

2. Install build dependencies:

       $ sudo apt-get install g++ cmake dpkg-dev libyajl-dev libssl-dev \
         libboost1.58-dev libboost-system1.58-dev libboost-atomic1.58-dev \
         libboost-chrono1.58-dev libboost-date-time1.58-dev \
         libboost-filesystem1.58-dev libboost-program-options1.58-dev \
         libboost-regex1.58-dev libboost-thread1.58-dev libboost-timer1.58-dev

## Debian 9 (stretch)

1. Install runtime dependencies:

       $ sudo apt-get install libssl1.0.2 libyajl2 libboost-filesystem1.62.0 \
         libboost-program-options1.62.0 libboost-system1.62.0 \
         libboost-thread1.62.0 ca-certificates

2. Install build dependencies:

       $ sudo apt-get install g++ cmake dpkg-dev libyajl-dev libssl1.0-dev \
         libboost1.62-dev libboost-system1.62-dev libboost-atomic1.62-dev \
         libboost-chrono1.62-dev libboost-date-time1.62-dev \
         libboost-filesystem1.62-dev libboost-program-options1.62-dev \
         libboost-regex1.62-dev libboost-thread1.62-dev libboost-timer1.62-dev

<!---
## CentOS 7

1. Install runtime dependencies:

       $ sudo yum install -y yajl
       $ (cd /tmp && \
          VENDOR_URL=http://testrepo.stackdriver.com/vendor/boost/x86_64 && \
          curl -O ${VENDOR_URL}/boost-system-1.54.0-1.el7.x86_64.rpm && \
          curl -O ${VENDOR_URL}/boost-thread-1.54.0-1.el7.x86_64.rpm)
       $ sudo rpm --nodeps -ivp /tmp/boost-{system,thread}-1.54.0-1.el7.x86_64.rpm

2. Install build dependencies:

       $ sudo yum install -y gcc-c++ cmake rpm-build yajl-devel openssl-devel
       $ (cd /tmp && \
          VENDOR_URL=http://testrepo.stackdriver.com/vendor/boost/x86_64 && \
          curl -O ${VENDOR_URL}/boost-devel-1.54.0-1.el7.x86_64.rpm && \
          curl -O ${VENDOR_URL}/boost-static-1.54.0-1.el7.x86_64.rpm)
       $ sudo rpm --nodeps -ivp /tmp/boost-{devel,static}-1.54.0-1.el7.x86_64.rpm

-->
## MacOS 10.12

1. Install runtime dependencies:

       $ brew install boost -c++11 && \
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

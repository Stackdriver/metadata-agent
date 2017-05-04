This is the Stackdriver metadata agent.

# Prerequisites

1. Install runtime dependencies:

       $ sudo apt-get install libyajl2 libboost-system1.55.0 libboost-thread1.55.0

2. Install build dependencies:

       $ sudo apt-get install g++ cmake libyajl-dev libssl-dev libboost1.55-dev \
         libboost-system1.55-dev libboost-atomic1.55-dev libboost-chrono1.55-dev \
         libboost-date-time1.55-dev libboost-filesystem1.55-dev \
         libboost-program-options1.55-dev libboost-regex1.55-dev \
         libboost-thread1.55-dev libboost-timer1.55-dev

## Ubuntu 16.04 special edition

1. Install runtime dependencies (Ubuntu 16.04 special edition):

       $ sudo apt-get install libyajl2 libboost-system1.58.0 libboost-thread1.58.0

2. Install build dependencies (Ubuntu 16.04 special edition):

       $ sudo apt-get install g++ cmake libyajl-dev libssl-dev libboost1.58-dev \
         libboost-system1.58-dev libboost-atomic1.58-dev libboost-chrono1.58-dev \
         libboost-date-time1.58-dev libboost-filesystem1.58-dev \
         libboost-program-options1.58-dev libboost-regex1.58-dev \
         libboost-thread1.58-dev libboost-timer1.58-dev

## CentOS 7 special edition

1. Download vendored packages (CentOS 7 special edition):

       $ (cd /tmp && \
          VENDOR_URL=http://testrepo.stackdriver.com/vendor/boost/x86_64 && \
          curl -O ${VENDOR_URL}/boost-devel-1.54.0-1.el7.x86_64.rpm && \
          curl -O ${VENDOR_URL}/boost-static-1.54.0-1.el7.x86_64.rpm && \
          curl -O ${VENDOR_URL}/boost-system-1.54.0-1.el7.x86_64.rpm && \
          curl -O ${VENDOR_URL}/boost-thread-1.54.0-1.el7.x86_64.rpm)

2. Install runtime dependencies (CentOS 7 special edition):

       $ sudo yum install -y yajl boost-system boost-thread
       $ sudo rpm --nodeps -ivp /tmp/boost-{system,thread}-1.54.0-1.el7.x86_64.rpm

3. Install build dependencies (CentOS 7 special edition):

       $ sudo yum install -y gcc-c++ cmake rpm-build yajl-devel openssl-devel
       $ sudo rpm --nodeps -ivp /tmp/boost-{devel,static}-1.54.0-1.el7.x86_64.rpm

# Building

1. Build the metadata agent:

       $ cd src
       $ make -j10

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

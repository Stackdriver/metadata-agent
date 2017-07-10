This is the Stackdriver metadata agent.

# Prerequisites

1. Install runtime dependencies:

       $ sudo apt-get install libyajl2

2. Install build dependencies:

       $ sudo apt-get install g++ libyajl-dev

## CentOS 7 special edition

1. Install runtime dependencies (CentOS 7 special edition):

       $ sudo yum install -y yajl

2. Install build dependencies (CentOS 7 special edition):

       $ sudo yum install -y gcc-c++ yajl-devel

# Building

1. Build the metadata agent:

       $ cd src
       $ make -j10

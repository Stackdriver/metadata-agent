This is the Stackdriver metadata agent.

# Prerequisites

1. Install build dependencies:

       $ sudo apt-get install g++

## CentOS 7 special edition

1. Install build dependencies (CentOS 7 special edition):

       $ sudo yum install -y gcc-c++

# Building

1. Build the metadata agent:

       $ cd src
       $ make -j10

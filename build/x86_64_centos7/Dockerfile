FROM centos:7

COPY puias-computational-x86_64.repo /etc/yum.repos.d/
RUN rpm --import http://springdale.math.ias.edu/data/puias/7/x86_64/os/RPM-GPG-KEY-puias \
    && yum update -y && yum install -y \
        gcc-c++ \
        git \
        cmake \
        make \
        yajl-devel \
        rpm-build \
        rpm-sign \
        openssl-devel \
        boost155-devel \
        expect \
    && yum -y clean all \
    && rm /etc/yum.repos.d/puias-computational-x86_64.repo
ENV BOOST_ROOT=/usr/local/boost/1.55.0
ENV LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$BOOST_ROOT/lib64"


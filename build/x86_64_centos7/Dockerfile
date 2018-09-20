FROM centos:7

RUN yum update -y && yum install -y \
        gcc-c++ \
        git \
        cmake \
        make \
        yajl-devel \
        rpm-build \
        rpm-sign \
        openssl-devel \
        expect \
    && yum -y clean all

RUN cd /tmp \
    && VENDOR_URL=http://testrepo.stackdriver.com/vendor/boost/x86_64 \
    && curl -O "${VENDOR_URL}/boost-{system,thread,devel,static}-1.54.0-1.el7.x86_64.rpm" \
    && rpm --nodeps -ivp boost-{system,thread,devel,static}-1.54.0-1.el7.x86_64.rpm

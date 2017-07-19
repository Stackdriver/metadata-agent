FROM ubuntu:xenial

ARG version=0.0.10-k8s-2
ADD https://storage.googleapis.com/stackdriver-container-alpha/deb/xenial/stackdriver-metadata_${version}.xenial.deb /stackdriver-metadata_${version}.xenial.deb
RUN apt-get update \
    && (dpkg -i /stackdriver-metadata_${version}.xenial.deb || true) \
    && apt-get install -f -y \
    && rm -rf /var/lib/apt/lists/*

CMD /opt/stackdriver/metadata/sbin/metadatad

EXPOSE 8000

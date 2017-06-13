FROM ubuntu:xenial

ADD https://storage.googleapis.com/stackdriver-container-alpha/deb/xenial/stackdriver-metadata_0.0.8-1.xenial.deb /stackdriver-metadata_0.0.8-1.xenial.deb
RUN apt-get update && (dpkg -i stackdriver-metadata_0.0.8-1.xenial.deb || apt-get install -f -y)

CMD /opt/stackdriver/metadata/sbin/metadatad

EXPOSE 8000
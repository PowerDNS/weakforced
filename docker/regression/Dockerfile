FROM ubuntu:jammy as wforce_regression

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get -y -f install \
               autoconf \
               automake \
               build-essential \
               libboost-all-dev \
               libcurl4-openssl-dev \
               libgetdns-dev \
               libhiredis-dev \
               libmaxminddb-dev \
               liblua5.1-0-dev \
               libluajit-5.1-dev \
               libprotobuf-dev \
               libssl-dev \
               libsodium-dev \
               libsystemd-dev \
               libyaml-cpp-dev \
               libtool \
               libjsoncpp-dev \
               libz-dev \
               uuid-dev \
               pkg-config \
               protobuf-compiler \
               asciidoctor \
               wget \
               nginx \
               docker \
               docker-compose \
               python3-pip \
               python3-venv \
               prometheus \
               geoip-bin \
               geoip-database \
               geoipupdate \
               net-tools \
               clang \
               cmake

ARG MAXMIND_LICENSE_KEY
RUN wget -O GeoLite2-City.mmdb.tar.gz "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-City&license_key=${MAXMIND_LICENSE_KEY}&suffix=tar.gz" || true
RUN wget -O GeoLite2-Country.mmdb.tar.gz "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-Country&license_key=${MAXMIND_LICENSE_KEY}&suffix=tar.gz" || true
RUN wget -O GeoLite2-ASN.mmdb.tar.gz "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-ASN&license_key=${MAXMIND_LICENSE_KEY}&suffix=tar.gz" || true
RUN gunzip GeoLite2-*.mmdb.tar.gz || true
RUN tar xvf GeoLite2-City.mmdb.tar || true
RUN tar xvf GeoLite2-Country.mmdb.tar || true
RUN tar xvf GeoLite2-ASN.mmdb.tar || true
RUN mv GeoLite2*/GeoLite2-*.mmdb /usr/share/GeoIP || true

RUN pip3 install bottle virtualenv

WORKDIR /wforce/
RUN mkdir /sdist

RUN git clone https://github.com/jupp0r/prometheus-cpp.git
RUN cd prometheus-cpp && git checkout tags/v1.0.1 -b v1.0.1
RUN cd prometheus-cpp && mkdir _build && cd _build && cmake .. -DBUILD_SHARED_LIBS=off -DENABLE_PULL=off -DENABLE_PUSH=off -DENABLE_COMPRESSION=off -DENABLE_TESTING=off && make && make install

RUN git clone https://github.com/drogonframework/drogon.git
RUN cd drogon && git checkout tags/v1.9.1 -b v1.9.1
RUN cd drogon && git submodule init && git submodule update && mkdir _build && cd _build && cmake .. -DBUILD_REDIS=OFF && make && make install

ADD CHANGELOG.md configure.ac ext LICENSE Makefile.am README.md NOTICE /wforce/
COPY m4/ /wforce/m4/
COPY ext/ /wforce/ext/
COPY docs/ /wforce/docs/
COPY regression-tests/ /wforce/regression-tests/
COPY wforce/ /wforce/wforce/
COPY common/ /wforce/common/
COPY trackalert/ /wforce/trackalert/
COPY docker/ /wforce/docker/
COPY elk/ /wforce/elk/

RUN rm -rf regression-tests/.venv  

CMD ["sleep", "infinity"]

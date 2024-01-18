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
               pandoc \
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

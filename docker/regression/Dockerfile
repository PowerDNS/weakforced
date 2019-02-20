FROM ubuntu:bionic as wforce

RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get -y -f install \
               autoconf \
               automake \
               libboost-all-dev \
               libcurl4-openssl-dev \
               libgeoip-dev \
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
               pkg-config \
               protobuf-compiler \
               pandoc \
               wget \
               docker \
               docker-compose \
               python3-pip \
               python3-venv \
               redis \
               geoip-bin \
               geoip-database \
               geoipupdate \
               net-tools \
               clang-4.0

# Disable Ipv6 for redis
RUN sed -i "s/bind .*/bind 127.0.0.1/g" /etc/redis/redis.conf
RUN echo "DatabaseDirectory /usr/share/GeoIP" >>/etc/GeoIP.conf
RUN geoipupdate -v
RUN pip3 install bottle virtualenv
RUN update-rc.d redis-server enable

WORKDIR /wforce/
RUN mkdir /sdist

ADD CHANGELOG.md configure.ac ext LICENSE Makefile.am README.md NOTICE trigger_policy_build.sh /wforce/
COPY m4/ /wforce/m4/
COPY ext/ /wforce/ext/
COPY docs/ /wforce/docs/
COPY regression-tests/ /wforce/regression-tests/
COPY wforce/ /wforce/wforce/
COPY common/ /wforce/common/
COPY trackalert/ /wforce/trackalert/
COPY report_api/ /wforce/report_api/
COPY docker/ /wforce/docker/
COPY elk/ /wforce/elk/

RUN rm -rf regression-tests/.venv  

CMD redis-server

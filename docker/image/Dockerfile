FROM debian:buster as wforce_build

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
               net-tools \
               clang \
               cmake

WORKDIR /wforce/
RUN mkdir /wforce/install

RUN git clone https://github.com/jupp0r/prometheus-cpp.git
RUN cd prometheus-cpp && git checkout tags/v0.9.0 -b v0.9.0
RUN cd prometheus-cpp && git submodule init && git submodule update && mkdir _build && cd _build && cmake .. -DBUILD_SHARED_LIBS=off && make && make install

ADD CHANGELOG.md configure.ac ext LICENSE Makefile.am README.md NOTICE trigger_policy_build.sh /wforce/
COPY m4/ /wforce/m4/
COPY ext/ /wforce/ext/
COPY docs/ /wforce/docs/
COPY wforce/ /wforce/wforce/
COPY common/ /wforce/common/
COPY trackalert/ /wforce/trackalert/
COPY report_api/ /wforce/report_api/
COPY docker/ /wforce/docker/
COPY elk/ /wforce/elk/

RUN autoreconf -ivf
RUN ./configure --prefix /usr --enable-trackalert --disable-systemd --disable-docker --with-luajit --sysconfdir=/etc/wforce CC=clang CXX=clang++
RUN make clean
RUN make install DESTDIR=/wforce/install

FROM debian:buster as wforce_image

COPY --from=wforce_build /wforce/install/ /

RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get -y -f install \
               libboost-date-time1.67.0 \
               libboost-regex1.67.0 \
               libboost-system1.67.0 \
               libboost-filesystem1.67.0 \
               libcurl4 \
               libgeoip1 \
               libgetdns10 \
               libhiredis0.14 \
               libluajit-5.1-2 \
               libmaxminddb0 \
               libreadline7 \
               libprotobuf17 \
               libssl1.1 \
               libsodium23 \
               libyaml-cpp0.6 \
               gnupg

RUN groupadd -g 1000 wforce && \
    useradd --uid 1000 -N -M -r --gid 1000 wforce && \
    chmod -R 0775 /etc/wforce && \
    chgrp -R 1000 /etc/wforce

COPY docker/image/docker-entrypoint.sh /usr/bin/docker-entrypoint.sh

RUN chmod 0775 /usr/bin/docker-entrypoint.sh

# We use tini to handle signals, reaping etc.
ENV TINI_VERSION v0.18.0
ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini /tini
ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini.asc /tini.asc
RUN gpg --batch --keyserver hkp://p80.pool.sks-keyservers.net:80 --recv-keys 595E85A6B1B4779EA4DAAEC70B588DFF0527A9B7 \
 && gpg --batch --verify /tini.asc /tini
RUN chmod 755 /tini

ARG build_date
ARG license
ARG git_revision
ARG version

LABEL org.label-schema.build-date="${build_date}" \
  org.label-schema.license="${license}" \
  org.label-schema.name="Weakforced" \
  org.label-schema.schema-version="1.0" \
  org.label-schema.url="https://powerdns.github.io/weakforced" \
  org.label-schema.usage="https://powerdns.github.io/weakforced" \
  org.label-schema.vcs-ref="${git_revision}" \
  org.label-schema.vcs-url="https://github.com/PowerDNS/weakforced" \
  org.label-schema.vendor="PowerDNS" \
  org.label-schema.version="${version}" \
  org.opencontainers.image.created="${build_date}" \
  org.opencontainers.image.documentation="https://powerdns.github.io/weakforced" \
  org.opencontainers.image.licenses="${license}" \
  org.opencontainers.image.revision="${git_revision}" \
  org.opencontainers.image.source="https://github.com/PowerDNS/weakforced" \
  org.opencontainers.image.title="Weakforced" \
  org.opencontainers.image.url="https://powerdns.github.io/weakforced" \
  org.opencontainers.image.vendor="PowerDNS" \
  org.opencontainers.image.version="${version}"

USER wforce:wforce

EXPOSE 8484

ENTRYPOINT ["/tini", "-v", "--", "/usr/bin/docker-entrypoint.sh"]
# Dummy overridable parameter parsed by entrypoint
CMD ["wfwrapper"]

FROM debian:bookworm-slim as wforce_build

RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get -y -f install \
               autoconf \
               automake \
               libboost-all-dev \
               libcurl4-openssl-dev \
               libgetdns-dev \
               libhiredis-dev \
               libmaxminddb-dev \
               liblua5.1-0-dev \
               libluajit2-5.1-dev \
               libprotobuf-dev \
               libssl-dev \
               libsodium-dev \
               libsystemd-dev \
               libyaml-cpp-dev \
               libjsoncpp-dev \
               libz-dev \
               prometheus-cpp-dev \
               uuid-dev \
               libtool \
               pkg-config \
               protobuf-compiler \
               pandoc \
               wget \
               docker \
               docker-compose \
               net-tools \
               clang \
               cmake

WORKDIR /wforce/
RUN mkdir /wforce/install

RUN git clone https://github.com/drogonframework/drogon.git
RUN cd drogon && git checkout tags/v1.7.4 -b v1.7.4
RUN cd drogon && git submodule init && git submodule update && mkdir _build && cd _build && cmake .. -DBUILD_REDIS=OFF -DCMAKE_BUILD_TYPE=Release -DBUILD_ORM=OFF && make && make install

ADD CHANGELOG.md configure.ac ext LICENSE Makefile.am README.md NOTICE /wforce/
COPY m4/ /wforce/m4/
COPY ext/ /wforce/ext/
COPY docs/ /wforce/docs/
COPY wforce/ /wforce/wforce/
COPY common/ /wforce/common/
COPY trackalert/ /wforce/trackalert/
COPY docker/Makefile.am /wforce/docker/
RUN  mkdir /wforce/docker/wforce_image
COPY docker/wforce_image/Makefile.am /wforce/docker/wforce_image
COPY elk/ /wforce/elk/

RUN autoreconf -ivf
RUN ./configure --prefix /usr --enable-trackalert --disable-systemd --disable-docker --with-luajit --sysconfdir=/etc/wforce CC=clang CXX=clang++
RUN make clean
RUN make install DESTDIR=/wforce/install

FROM debian:bookworm-slim as wforce_image

WORKDIR /wforce/

COPY --from=wforce_build /wforce/install/ /

# Remove the default wforce.conf and trackalert.conf files
RUN rm /etc/wforce/wforce.conf /etc/wforce/trackalert.conf

RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get -y -f install \
               libboost-date-time1.74.0 \
               libboost-regex1.74.0 \
               libboost-system1.74.0 \
               libboost-filesystem1.74.0 \
               libcurl4 \
               libgetdns10 \
               libhiredis0.14 \
               libluajit2-5.1-2 \
               libmaxminddb0 \
               libreadline8 \
               libprotobuf32 \
               libprometheus-cpp-core1.0 \
               libssl3 \
               libsodium23 \
               libyaml-cpp0.7 \
               libjsoncpp-dev \
               libz-dev \
               uuid-dev \
               gnupg \
               python3 \
               python3-jinja2 \
               net-tools \
               tini

RUN groupadd -g 1000 wforce && \
    useradd --uid 1000 -N -M -r --gid 1000 wforce && \
    chmod -R 0775 /etc/wforce && \
    chgrp -R 1000 /etc/wforce

COPY docker/wforce_image/docker-entrypoint.sh /usr/bin/docker-entrypoint.sh
COPY docker/wforce_image/create_config.sh /usr/bin/create_config.sh
COPY docker/wforce_image/wforce.conf.j2 /etc/wforce
COPY docker/wforce_image/trackalert.conf.j2 /etc/wforce

RUN chmod 0775 /usr/bin/docker-entrypoint.sh
RUN chmod 0775 /usr/bin/create_config.sh

RUN rm -rf /wforce/*

ARG build_date
ARG license
ARG git_revision
ARG version

ENV WFORCE_VERBOSE=0
ENV WFORCE_HTTP_PASSWORD=
ENV WFORCE_HTTP_PORT=8084
ENV WFORCE_LOGSTASH_URL=
ENV WFORCE_CONFIG_FILE=
ENV TRACKALERT=
ENV TRACKALERT_HTTP_PORT=8085
ENV TRACKALERT_HTTP_PASSWORD=
ENV TRACKALERT_CONFIG_FILE=

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

EXPOSE 8084

ENTRYPOINT ["/usr/bin/tini", "-v", "--", "/usr/bin/docker-entrypoint.sh"]
# Dummy overridable parameter parsed by entrypoint
CMD ["wfwrapper"]

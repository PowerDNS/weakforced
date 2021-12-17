FROM debian:bullseye-slim as wforce_build

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
               libjsoncpp-dev \
               libz-dev \
               uuid-dev \
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

RUN git clone https://github.com/drogonframework/drogon.git
RUN cd drogon && git checkout tags/v1.7.1 -b v1.7.1
RUN cd drogon && git submodule init && git submodule update && mkdir _build && cd _build && cmake .. -DBUILD_REDIS=OFF -DCMAKE_BUILD_TYPE=Release && make && make install

ADD CHANGELOG.md configure.ac ext LICENSE Makefile.am README.md NOTICE /wforce/
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

FROM debian:bullseye-slim as wforce_image

WORKDIR /wforce/

COPY --from=wforce_build /wforce/install/ /

COPY --from=wforce_build /wforce/report_api /wforce/report_api

RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get -y -f install \
               libboost-date-time1.74.0 \
               libboost-regex1.74.0 \
               libboost-system1.74.0 \
               libboost-filesystem1.74.0 \
               libcurl4 \
               libgeoip1 \
               libgetdns10 \
               libhiredis0.14 \
               libluajit-5.1-2 \
               libmaxminddb0 \
               libreadline8 \
               libprotobuf23 \
               libssl1.1 \
               libsodium23 \
               libyaml-cpp0.6 \
               libjsoncpp-dev \
               libz-dev \
               uuid-dev \
               gnupg \
               python3 \
               python3-pip \
               python3-venv \
               python3-jinja2 \
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

ARG VENV_DIR=/usr/share/wforce-report-api
RUN python3 -m venv --symlinks $VENV_DIR
RUN cd report_api && $VENV_DIR/bin/python $VENV_DIR/bin/pip install .
RUN mkdir /etc/wforce-report-api
RUN cp report_api/helpers/wforce-report-api-webserver /usr/bin/wforce-report-api-webserver && chmod 755 /usr/bin/wforce-report-api-webserver 
RUN cp report_api/instance/report.cfg /etc/wforce-report-api/wforce-report-api-instance.conf && chmod 644 /etc//wforce-report-api/wforce-report-api-instance.conf
RUN cp report_api/helpers/wforce-report-api-docker.conf /etc/wforce-report-api/wforce-report-api-web.conf && chmod 755 /etc/wforce-report-api/wforce-report-api-web.conf
RUN export python3_version=$(python3 -V | awk '{print $2}' | awk 'BEGIN{FS="."};{print $1"."$2}') && sed -i -e s:\<python_version\>:$python3_version: /etc/wforce-report-api/wforce-report-api-web.conf
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

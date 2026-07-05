# Xapiand release image: builds the current source from scratch and ships a
# slim runtime. Build from the repository root:
#   docker build -t ghcr.io/kronuz/xapiand:latest .
#
# The build fetches the standalone Kronuz libraries through CMake FetchContent,
# so the builder stage needs network access.

FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN set -ex \
  && apt-get update \
  && apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    g++-13 \
    cmake \
    ninja-build \
    pkg-config \
    perl \
    tcl \
    libasio-dev \
    libzstd-dev \
    libicu-dev \
    uuid-dev \
    zlib1g-dev \
    libcap-dev \
  && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-13 CXX=g++-13

COPY . /usr/src/Xapiand
WORKDIR /usr/src/Xapiand/build
RUN cmake -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DASIO_INCLUDE_DIR=/usr/include \
      .. \
  && ninja \
  && DESTDIR=/opt/stage ninja install


FROM ubuntu:24.04

LABEL org.opencontainers.image.source="https://github.com/Kronuz/Xapiand"
LABEL org.opencontainers.image.description="Xapiand: a RESTful search engine"
LABEL org.opencontainers.image.licenses="MIT"

ENV DEBIAN_FRONTEND=noninteractive
RUN set -ex \
  && apt-get update \
  && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    libicu74 \
    libuuid1 \
    zlib1g \
    libzstd1 \
    libcap2 \
  && rm -rf /var/lib/apt/lists/* \
  && groupadd -r xapiand \
  && useradd -r -g xapiand -d /var/db/xapiand -s /usr/sbin/nologin xapiand \
  && mkdir -p /var/db/xapiand \
  && chown -R xapiand:xapiand /var/db/xapiand

COPY --from=builder /opt/stage/usr /usr
COPY contrib/docker/xapiand/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 8880
ENTRYPOINT ["/entrypoint.sh"]
CMD ["xapiand"]

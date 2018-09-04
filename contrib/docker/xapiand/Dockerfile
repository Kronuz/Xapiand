# docker build -t kronuz/xapiand contrib/docker/xapiand && docker push kronuz/xapiand
FROM kronuz/xapian as builder

# Build Xapiand
RUN CONFIG="\
    -DCMAKE_INSTALL_PREFIX:PATH=/usr \
  " \
  && apk add libuuid \
  && apk add --no-cache --virtual .build-deps \
    ninja \
    cmake \
    git \
    zlib-dev \
    util-linux-dev \
  && mkdir -p /usr/src \
  && git clone --single-branch --depth=1 "https://github.com/Kronuz/Xapiand" /usr/src/Xapiand \
  && mkdir /usr/src/Xapiand/build \
  && cd /usr/src/Xapiand/build \
  && cmake -GNinja $CONFIG .. \
  && ninja install \
  && rm -rf /usr/src/Xapiand \
  && apk del .build-deps \
  && apk del build-clang

# Xapiand image
FROM alpine:3.8

MAINTAINER Kronuz

COPY --from=builder /usr /usr
COPY ./entrypoint.sh /

RUN mkdir -p /var/db \
  && addgroup -S xapiand \
  && adduser -D -S -h /var/db/xapiand -s /sbin/nologin -G xapiand xapiand \
  && chmod +x /entrypoint.sh

EXPOSE 8880

ENTRYPOINT ["/entrypoint.sh"]
CMD ["-v"]

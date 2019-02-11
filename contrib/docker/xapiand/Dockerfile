#################################################################################
#       ___
#  __  /  /          _                 _
#  \ \/  /__ _ _ __ (_) __ _ _ __   __| |
#   \   // _` | '_ \| |/ _` | '_ \ / _` |
#   /   \ (_| | |_) | | (_| | | | | (_| |
#  / /\__\__,_| .__/|_|\__,_|_| |_|\__,_|
# /_/         |_|
#
# Build using:
# docker build -t dubalu/xapiand:latest contrib/docker/xapiand
# docker tag dubalu/xapiand:latest dubalu/xapiand:$(docker run --rm dubalu/xapiand:latest --version)
# docker push dubalu/xapiand:$(docker run --rm dubalu/xapiand:latest --version)
# docker push dubalu/xapiand:latest

FROM dubalu/clang as builder

RUN apk add libuuid \
  && apk add --no-cache --virtual .build-deps \
    git \
    zlib-dev \
    # Xapian specific:
    perl \
    tcl \
    # Xapiand specific:
    ninja \
    cmake \
    util-linux-dev

# Build Xapiand using clang (from Kronuz git repo using layer cache buster)
ADD https://api.github.com/repos/Kronuz/Xapiand/git/refs/heads/master xapiand-version.json
RUN CONFIG="\
    -DCMAKE_INSTALL_PREFIX:PATH=/usr \
  " \
  && mkdir -p /usr/src \
  && git clone --single-branch --depth=1 "https://github.com/Kronuz/Xapiand" /usr/src/Xapiand \
  && mkdir /usr/src/Xapiand/build \
  && cd /usr/src/Xapiand/build \
  && cmake -GNinja $CONFIG .. \
  && ninja install

RUN rm -rf /usr/src/Xapiand \
  && apk del .build-deps \
  && apk del build-clang

# Xapiand image
FROM alpine:3.8

MAINTAINER Kronuz

COPY --from=builder /usr /usr
COPY ./entrypoint.sh /

RUN apk add libuuid \
  && mkdir -p /var/db \
  && addgroup -S xapiand \
  && adduser -D -S -h /var/db/xapiand -s /sbin/nologin -G xapiand xapiand \
  && chmod +x /entrypoint.sh

EXPOSE 8880

ENTRYPOINT ["/entrypoint.sh"]
CMD ["xapiand"]

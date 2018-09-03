# docker build -t kronuz/xapiand contrib/docker/xapiand && docker push kronuz/xapiand
FROM kronuz/xapian

# Build xapiand
RUN apk add --no-cache --virtual .build-deps \
    cmake \
    git \
    zlib-dev \
    util-linux-dev \
  && mkdir -p /usr/src \
  && git clone --single-branch --depth=1 "https://github.com/Kronuz/Xapiand" /usr/src/Xapiand \
  && mkdir /usr/src/Xapiand/build \
  && cd /usr/src/Xapiand/build \
  && cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr .. \
  && make install \
  && rm -rf /usr/src/Xapiand \
  && apk del .build-deps

# Remove clang
RUN apk del build-clang

EXPOSE 8880

CMD ["xapiand"]

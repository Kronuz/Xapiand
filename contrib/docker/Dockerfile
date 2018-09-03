# docker build -t kronuz/xapiand .
FROM kronuz/clang

RUN apk add --no-cache --virtual .build-deps \
    make \
    autoconf \
    automake \
    libtool \
    cmake \
    git \
    perl \
    tcl \
    zlib-dev \
    util-linux-dev \
  && mkdir -p /usr/src \
  \
  # build xapian (using clang)
  && git clone -b RELEASE/1.4 --single-branch --depth 1 "https://github.com/xapian/xapian.git" /usr/src/xapian \
  && cd /usr/src/xapian/xapian-core \
  && ./preautoreconf \
  && autoreconf --force --install -Im4 \
  && CC=clang CXX=clang++ ./configure \
    --disable-dependency-tracking \
    --disable-documentation \
    --enable-maintainer-mode \
    --prefix=/usr \
  && make install \
  \
  # build xapiand
  && git clone --single-branch --depth=1 "https://github.com/Kronuz/Xapiand" /usr/src/Xapiand \
  && mkdir /usr/src/Xapiand/build \
  && cd /usr/src/Xapiand/build \
  && cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr .. \
  && make install \
  \
  # cleanup
  && rm -rf /usr/src/xapian \
  && rm -rf /usr/src/Xapiand \
  && apk del .build-deps build-clang

EXPOSE 8880

CMD ["xapiand"]

# docker build -t kronuz/xapian contrib/docker/xapian && docker push kronuz/xapian
FROM kronuz/clang

# Build xapian (using clang)
RUN apk add --no-cache --virtual .build-deps \
    make \
    autoconf \
    automake \
    libtool \
    git \
    perl \
    tcl \
    zlib-dev \
  && mkdir -p /usr/src \
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
  && rm -rf /usr/src/xapian \
  && apk del .build-deps

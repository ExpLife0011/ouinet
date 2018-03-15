FROM debian:stretch AS builder
RUN apt-get update && apt-get install -y \
    autoconf \
    automake \
    autopoint \
    build-essential \
    cmake \
    gettext \
    git \
    libboost-coroutine-dev \
    libboost-date-time-dev \
    libboost-dev \
    libboost-filesystem-dev \
    libboost-program-options-dev \
    libboost-regex-dev \
    libboost-system-dev \
    libboost-test-dev \
    libboost-thread-dev \
    libgcrypt-dev \
    libidn11-dev \
    libssl-dev \
    libtool \
    libunistring-dev \
    pkg-config \
    rsync \
    texinfo \
    wget \
    zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /usr/local/src
RUN git clone --recursive https://github.com/equalitie/ouinet.git
WORKDIR /opt/ouinet
RUN cmake /usr/local/src/ouinet \
 && make

FROM debian:stretch
# To get the list of library packages, enter the build directory and execute:
#
#     ldd injector client $(find . -name '*.so' | grep -v '\.libs') \
#         | sed -En 's#^.* => (/lib/.*|/usr/lib/.*) \(.*#\1#p' | sort -u \
#         | (while read l; do dpkg -S $l; done) | cut -f1 -d: | sort -u
#
RUN apt-get update && apt-get install -y \
    libboost-atomic1.62.0 \
    libboost-chrono1.62.0 \
    libboost-context1.62.0 \
    libboost-coroutine1.62.0 \
    libboost-date-time1.62.0 \
    libboost-filesystem1.62.0 \
    libboost-program-options1.62.0 \
    libboost-regex1.62.0 \
    libboost-system1.62.0 \
    libboost-test1.62.0 \
    libboost-thread1.62.0 \
    libboost-timer1.62.0 \
    libc6 \
    libgcc1 \
    libgcrypt20 \
    libgpg-error0 \
    libicu57 \
    libltdl7 \
    libssl1.1 \
    libstdc++6 \
    libunistring0 \
    zlib1g \
 && rm -rf /var/lib/apt/lists/*

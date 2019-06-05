# Copyright (C) 2019  Christian Berger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Part to build video-vpx-replay.
FROM alpine:3.7 as builder
MAINTAINER Christian Berger "christian.berger@gu.se"

RUN apk update && \
    apk --no-cache add \
        diffutils \
        cmake \
        g++ \
        git \
        libx11-dev \
        make \
        perl \
        wget \
        yasm
RUN cd tmp && \
    git clone https://chromium.googlesource.com/libyuv/libyuv && \
    cd libyuv &&\
    git reset --hard 5b6042fa0d211ebbd8b477c7f3855977c7973048 && \
    make -f linux.mk libyuv.a && cp libyuv.a /usr/lib && cd include && cp -r * /usr/include
RUN cd tmp && \
    git clone --depth 1 --branch v1.8.0 https://github.com/webmproject/libvpx.git && \
    mkdir build && cd build && \
    ../libvpx/configure --disable-docs --disable-tools --enable-vp8 --enable-vp9 --enable-libyuv --disable-unit-tests --disable-webm-io --disable-postproc && \
    make -j4 && make install
ADD . /opt/sources
WORKDIR /opt/sources
RUN mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/tmp .. && \
    make && make install


# Part to deploy video-vpx-replay.
FROM alpine:3.7
MAINTAINER Christian Berger "christian.berger@gu.se"

RUN echo http://dl-4.alpinelinux.org/alpine/edge/main > /etc/apk/repositories && \
    echo http://dl-4.alpinelinux.org/alpine/edge/community >> /etc/apk/repositories && \
    echo http://dl-4.alpinelinux.org/alpine/edge/testing >> /etc/apk/repositories && \
    apk update && \
    apk --no-cache add \
        libx11

WORKDIR /usr/bin
COPY --from=builder /tmp/bin/video-vpx-replay .
ENTRYPOINT ["/usr/bin/video-vpx-replay"]


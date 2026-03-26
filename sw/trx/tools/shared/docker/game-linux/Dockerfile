# TRX building toolchain for Linux.
#
# This is a multi-stage Docker image. It is designed to keep the final image
# size low. Each stage builds an external dependency. The final stage takes the
# artifacts (binaries, includes etc.) from previous stages and installs all the
# tools necessary to build TRX.

FROM ubuntu:latest AS base

# don't prompt during potential installation/update of tzinfo
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y \
        git \
        make



# pcre2
FROM base AS pcre2
RUN apt-get install -y git gcc autoconf libtool
RUN git clone https://github.com/PCRE2Project/pcre2
RUN cd pcre2 \
    && autoreconf -fi \
    && ./configure \
        --prefix=/ext/ \
    && make -j 4 \
    && make install



# libbacktrace
FROM base AS backtrace
RUN apt-get install -y git gcc autoconf libtool
RUN git clone https://github.com/LostArtefacts/libbacktrace/
RUN cd libbacktrace \
    && autoreconf -fi \
    && ./configure \
        --prefix=/ext/ \
    && make -j 4 \
    && make install



# libav
FROM base AS libav
RUN apt-get install -y \
    nasm \
    gcc \
    zlib1g-dev
RUN git clone \
    https://github.com/FFmpeg/FFmpeg
RUN cd FFmpeg && git checkout 066432ebcf
COPY ./tools/ffmpeg_flags.txt /tmp/ffmpeg_flags.txt
RUN cd FFmpeg \
    && ./configure \
        --arch=x86 \
        --prefix=/ext/ \
        --enable-static \
        --disable-shared \
        $(cat /tmp/ffmpeg_flags.txt) \
    && make -j 4 \
    && make install



# SDL
FROM base AS sdl
RUN git clone https://github.com/libsdl-org/SDL -b SDL2
RUN apt-get install -y \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libpulse-dev \
    libasound2-dev \
    libxkbcommon-dev \
    libwayland-client0 \
    libwayland-cursor0 \
    libwayland-dev \
    libwayland-egl1 \
    automake \
    gcc \
    libxrandr-dev \
    libxext-dev
RUN cd SDL \
    && aclocal -I acinclude \
    && autoconf \
    && mkdir sdl_build \
    && cd sdl_build \
    && ../configure \
        --prefix=/ext/ \
        --enable-video-x11-xrandr \
        --enable-shared \
        --enable-static \
    && make -j 4 \
    && make install



# GLEW
FROM base AS glew
RUN git clone https://github.com/nigels-com/glew.git
RUN apt-get install -y \
    build-essential \
    libxmu-dev \
    libxi-dev \
    libgl-dev \
    python3
RUN cd glew/auto \
    && PYTHON=python3 make
RUN mkdir -p /ext/lib \
    && export \
        GLEW_NO_GLU=-DGLEW_NO_GLU \
        GLEW_DEST=/ext \
    && cd glew \
    && make \
    && make install
RUN sed -i "s/Cflags: .*/\\0 -DGLEW_STATIC /" /ext/lib/pkgconfig/glew.pc

## Lua
FROM base AS lua
RUN apt-get update && apt-get install -y wget build-essential libreadline-dev
RUN wget https://www.lua.org/ftp/lua-5.4.8.tar.gz \
    && tar xzf lua-5.4.8.tar.gz \
    && cd lua-5.4.8 \
    && make linux \
    && make INSTALL_TOP=/ext install

# Provide pkg-config file for Lua
RUN mkdir -p /ext/lib/pkgconfig
COPY tools/shared/docker/lua.pc /ext/lib/pkgconfig/lua.pc





# TRX
FROM base

# set the build dir - actual files are mounted with a Docker volume
RUN mkdir /app
WORKDIR /app

# package dependencies
RUN apt-get install -y \
    zlib1g-dev \
    libgl1-mesa-dev

# tooling dependencies
# configure pkgconfig manually
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=967969
ENV PKG_CONFIG_LIBDIR=/ext/lib/
ENV PKG_CONFIG_PATH=/ext/lib/pkgconfig/
RUN apt-get install -y \
        pkg-config \
        git \
        ccache \
        python3-pip \
    && python3 -m pip install --break-system-packages \
        pyjson5 \
        meson \
        ninja
# Regular dependencies
RUN apt-get install -y \
    upx \
    uthash-dev

## manually built dependencies
COPY --from=lua /ext/ /ext/
COPY --from=libav /ext/ /ext/
COPY --from=sdl /ext/ /ext/
COPY --from=backtrace /ext/ /ext/
COPY --from=pcre2 /ext/ /ext/
COPY --from=glew /ext/ /ext/

ENV PYTHONPATH=/app/tools/
ENV CCACHE_BASEDIR=/app/
ENV CCACHE_COMPILERCHECK=content
ENV CCACHE_DIR=/app/.cache/ccache/linux
ENV CC="ccache gcc"
ENV CXX="ccache g++"
ENTRYPOINT ["/app/tools/shared/docker/game-linux/entrypoint.sh"]

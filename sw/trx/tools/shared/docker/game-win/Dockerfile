# TRX building toolchain.
#
# This is a multi-stage Docker image. It is designed to keep the final image
# size low. Each stage builds an external dependency. The final stage takes the
# artifacts (binaries, includes etc.) from previous stages and installs all the
# tools necessary to build TRX.

# MinGW
FROM ubuntu:latest AS mingw

# don't prompt during potential installation/update of tzinfo
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y \
        gcc \
        gcc-mingw-w64-i686 \
        g++-mingw-w64-i686 \
        git \
        make



# pcre
FROM mingw AS pcre2
RUN git clone https://github.com/PCRE2Project/pcre2
RUN apt-get -y install libtool
RUN cd pcre2 \
    && autoreconf -fi \
    && ./configure \
        --host=i686-w64-mingw32 \
        --prefix=/ext/ \
    && make -j 4 \
    && make install



# zlib
FROM mingw AS zlib
RUN git clone https://github.com/madler/zlib --branch=v1.3.1
RUN cd zlib \
    && make -f win32/Makefile.gcc \
        SHARED_MODE=1 \
        BINARY_PATH=/ext/bin \
        INCLUDE_PATH=/ext/include \
        LIBRARY_PATH=/ext/lib \
        PREFIX=i686-w64-mingw32- \
        -j 4 install



# libav
FROM mingw AS libav
RUN apt-get install -y \
    nasm
RUN git clone \
    https://github.com/FFmpeg/FFmpeg
RUN cd FFmpeg && git checkout 066432ebcf
COPY --from=zlib /ext/ /usr/i686-w64-mingw32/
COPY ./tools/ffmpeg_flags.txt /tmp/ffmpeg_flags.txt
RUN cd FFmpeg \
    && ./configure \
        --arch=x86 \
        --target-os=mingw32 \
        --cross-prefix=i686-w64-mingw32- \
        --prefix=/ext/ \
        --cc=i686-w64-mingw32-gcc \
        --cxx=i686-w64-mingw32-g++ \
        --host-cc=gcc \
        --strip=i686-w64-mingw32-strip \
        --pkg-config=i686-w64-mingw32-pkg-config \
        --enable-static \
        --disable-shared \
        $(cat /tmp/ffmpeg_flags.txt) \
    && make -j 4 \
    && make install



# SDL
FROM mingw AS sdl
RUN git clone https://github.com/libsdl-org/SDL -b SDL2
RUN apt-get install -y automake
RUN cd SDL \
    && aclocal -I acinclude \
    && autoconf \
    && mkdir build \
    && cd build \
    && ../configure \
        --host=i686-w64-mingw32 \
        --build=i686-pc-mingw32 \
        --prefix=/ext/ \
        --enable-shared \
        --enable-static \
    && make -j 4 \
    && make install

# SDL3 - Compiles fine, but meson cannot find it.
# TODO - fix that once SDL reaches maturity and releases 3.x as a stable version
# RUN git clone https://github.com/libsdl-org/SDL
# RUN apt-get install -y cmake
# RUN cd SDL \
#     && mkdir build \
#     && cd build \
#     && cmake .. \
#         --toolchain build-scripts/cmake-toolchain-mingw64-i686.cmake \
#         -DCMAKE_INSTALL_PREFIX=/ext/  \
#         -DCMAKE_BUILD_TYPE=Release \
#         -DSDL_VULKAN=ON \
#         -DSDL_TEST=ON \
#     && cmake --build . \
#     && make -j 4 \
#     && make install



# uthash
FROM mingw AS uthash

RUN mkdir /ext/
WORKDIR /tmp/

RUN apt-get install -y wget xz-utils
RUN wget https://github.com/troydhanson/uthash/archive/v2.3.0.tar.gz
RUN tar -xvf v2.3.0.tar.gz
RUN cp -rL uthash-2.3.0/* /ext/



# GLEW
FROM mingw AS glew
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
        SYSTEM=linux-mingw32 \
        GLEW_NO_GLU=-DGLEW_NO_GLU \
        GLEW_DEST=/ext \
    && cd glew \
    && make \
    && make install
RUN sed -i "s/Cflags: .*/\\0 -DGLEW_STATIC/" /ext/lib/pkgconfig/glew.pc

## Lua
FROM mingw AS lua
RUN apt-get update && apt-get install -y wget build-essential
RUN wget https://www.lua.org/ftp/lua-5.4.8.tar.gz \
    && tar xzf lua-5.4.8.tar.gz \
    && cd lua-5.4.8 \
    && make mingw CROSS=i686-w64-mingw32- CC="i686-w64-mingw32-gcc" \
    && make INSTALL_TOP=/ext TO_BIN="lua.exe luac.exe" install

# Provide pkg-config file for Lua
RUN mkdir -p /ext/lib/pkgconfig
COPY tools/shared/docker/lua.pc /ext/lib/pkgconfig/lua.pc




# TRX
FROM mingw

# set the build dir - actual files are mounted with a Docker volume
RUN mkdir /app
WORKDIR /app

# system dependencies
# configure pkgconfig manually
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=967969
RUN apt-get install -y \
        mingw-w64-tools \
        pkg-config \
        upx \
        ccache \
        python3-pip \
    && python3 -m pip install --break-system-packages \
        pyjson5 \
        meson \
        ninja

COPY --from=lua /ext/ /ext/
COPY --from=pcre2 /ext/ /ext/
COPY --from=zlib /ext/ /ext/
COPY --from=libav /ext/ /ext/
COPY --from=sdl /ext/ /ext/
COPY --from=uthash /ext/ /ext/
COPY --from=glew /ext/ /ext/

ENV PKG_CONFIG_LIBDIR=/ext/lib/
ENV PKG_CONFIG_PATH=/ext/lib/pkgconfig/
ENV C_INCLUDE_PATH=/ext/include/
ENV PYTHONPATH=/app/tools/
ENV CCACHE_BASEDIR=/app/
ENV CCACHE_COMPILERCHECK=content
ENV CCACHE_DIR=/app/.cache/ccache/win
ENTRYPOINT ["/app/tools/shared/docker/game-win/entrypoint.sh"]

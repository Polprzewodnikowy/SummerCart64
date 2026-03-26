FROM python:3.12-slim-bookworm

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    gnupg \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm-archive-keyring.gpg
RUN echo 'deb [signed-by=/usr/share/keyrings/llvm-archive-keyring.gpg] https://apt.llvm.org/bookworm llvm-toolchain-bookworm-22 main' > /etc/apt/sources.list.d/llvm.list
RUN apt-get update && apt-get install -y --no-install-recommends \
    clang-format-22 \
    && apt-get purge -y --auto-remove gnupg xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN ln -s /usr/bin/clang-format-22 /usr/local/bin/clang-format
RUN just_version='1.40.0' && \
    curl -fsSL "https://github.com/casey/just/releases/download/${just_version}/just-${just_version}-x86_64-unknown-linux-musl.tar.gz" \
    | tar -xz -C /usr/local/bin just && \
    chmod +x /usr/local/bin/just
RUN python3 -m pip install --no-cache-dir \
    prek \
    pyjson5

ENV PATH="/usr/local/bin:${PATH}"

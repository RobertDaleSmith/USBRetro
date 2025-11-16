FROM docker.io/debian:bookworm-slim

ARG DEBIAN_FRONTEND=noninteractive
RUN apt update && \
    apt install -y --no-install-recommends \
      build-essential \
      cmake \
      git \
      gcc-arm-none-eabi \
      libnewlib-arm-none-eabi \
      libstdc++-arm-none-eabi-newlib \
      python3 \
      python3-pip \
      vim && \
    apt autoremove -y && \
    apt clean && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /root/workspace/USBRetro
WORKDIR /root/workspace/USBRetro

# Copy project with submodules (pico-sdk already initialized by host)
COPY . .

# Set pico-sdk path to local submodule
ENV PICO_SDK_PATH=/root/workspace/USBRetro/src/lib/pico-sdk

WORKDIR /root/workspace/USBRetro/src

CMD ["/bin/bash"]

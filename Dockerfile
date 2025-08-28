FROM docker.io/debian:bookworm-slim

# Use archived repos for EOL Debian buster
RUN sed -i \
    -e 's|http://deb.debian.org/debian|http://archive.debian.org/debian|g' \
    -e 's|http://deb.debian.org/debian-security|http://archive.debian.org/debian-security|g' \
    -e 's|http://security.debian.org/debian-security|http://archive.debian.org/debian-security|g' \
    /etc/apt/sources.list \
 && printf 'Acquire::Check-Valid-Until "false";\n' > /etc/apt/apt.conf.d/99no-check-valid-until \
 && apt-get -o Acquire::Check-Valid-Until=false update

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
WORKDIR /root/workspace

RUN git clone https://github.com/raspberrypi/pico-sdk.git
ENV PICO_SDK_PATH=/root/workspace/pico-sdk
WORKDIR /root/workspace/pico-sdk
RUN git submodule update --init lib/tinyusb

WORKDIR /root/workspace/USBRetro
COPY . .
RUN git submodule update --init

WORKDIR /root/workspace/USBRetro/src

CMD ["/bin/bash"]

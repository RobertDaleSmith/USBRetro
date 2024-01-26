FROM docker.io/debian:buster-slim

ARG DEBIAN_FRONTEND=noninteractive
RUN apt update
RUN apt install -y build-essential cmake git
RUN apt install -y gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
RUN apt install -y python3 python3-pip
RUN apt install -y vim
RUN apt autoremove && apt clean

RUN mkdir -p /root/workspace/USBRetro
WORKDIR /root/workspace

RUN git clone --branch 1.5.1 https://github.com/raspberrypi/pico-sdk.git
ENV PICO_SDK_PATH=/root/workspace/pico-sdk
WORKDIR /root/workspace/pico-sdk/lib/tinyusb
RUN git submodule init
RUN git submodule update
RUN git checkout 4b3b401ce

WORKDIR /root/workspace/USBRetro
COPY . .
RUN git submodule update --init

WORKDIR /root/workspace/USBRetro/src

CMD ["/bin/bash"]

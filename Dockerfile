FROM docker.io/debian:bookworm-slim

ARG DEBIAN_FRONTEND=noninteractive
RUN apt update
RUN apt install -y build-essential cmake git
RUN apt install -y gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
RUN apt install -y python3 python3-pip
RUN apt install -y vim
RUN apt autoremove && apt clean

RUN mkdir -p /root/workspace
WORKDIR /root/workspace

RUN git clone --branch 1.5.1 https://github.com/raspberrypi/pico-sdk.git
ENV PICO_SDK_PATH=/root/workspace/pico-sdk
WORKDIR /root/workspace/pico-sdk/lib/tinyusb
RUN git submodule init
RUN git submodule update
RUN git checkout master

WORKDIR /root/workspace
RUN git clone --branch USBRETRO_V1_0_3 https://github.com/RobertDaleSmith/USBRetro.git
WORKDIR /root/workspace/USBRetro
RUN git submodule init
RUN git submodule update

WORKDIR /root/workspace/USBRetro/src
RUN sh build.sh

WORKDIR /root/workspace/USBRetro/src/build
RUN cmake ..
RUN make usbretro_ngc

CMD ["/bin/bash"]

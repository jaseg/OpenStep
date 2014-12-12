FROM ubuntu
MAINTAINER github@jaseg.net

RUN DEBIAN_FRONTEND=noninteractive apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y binutils-msp430 gcc-msp430 msp430-libc mspdebug srecord build-essential
RUN mkdir /root/build

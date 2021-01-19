FROM ubuntu:20.04

RUN apt-get -qq update
RUN apt-get -qq install -y wget unzip build-essential cmake ninja-build git

RUN mkdir tools

# Install gcc_arm_none_eabi_10_2020_q4_major
RUN wget -q https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2
RUN tar -xjf gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2 -C /tools
RUN rm gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2
ENV PATH="/tools/gcc-arm-none-eabi-10-2020-q4-major/bin:$PATH"

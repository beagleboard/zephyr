#!/bin/bash -ex
sudo apt update
sudo apt install --no-install-recommends -y \
    gperf \
    gcc python3-dev \
    ccache dfu-util \
    libsdl2-dev \
    libxml2-dev libxslt1-dev libssl-dev libjpeg62-turbo-dev libmagic1 \
    libtool-bin autoconf automake libusb-1.0-0-dev \
    python3-tk python3-virtualenv
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.15.1/zephyr-sdk-0.15.1_linux-aarch64_minimal.tar.gz
tar xf zephyr-sdk-0.15.1_linux-aarch64_minimal.tar.gz
rm zephyr-sdk-0.15.1_linux-aarch64_minimal.tar.gz
./zephyr-sdk-0.15.1/setup.sh -t arm-zephyr-eabi -c
python3 -m virtualenv zephyr-beagle-cc1352-env
echo "export ZEPHYR_TOOLCHAIN_VARIANT=zephyr" >> ./zephyr-beagle-cc1352-env/bin/activate
echo "export ZEPHYR_SDK_INSTALL_DIR=$(pwd)/zephyr-sdk-0.15.1" >> ./zephyr-beagle-cc1352-env/bin/activate
echo "export ZEPHYR_BASE=$(pwd)/zephyr-beagle-cc1352-sdk/zephyr" >> ./zephyr-beagle-cc1352-env/bin/activate
echo 'export PATH=$ZEPHYR_BASE/scripts:$PATH' >> ./zephyr-beagle-cc1352-env/bin/activate
source zephyr-beagle-cc1352-env/bin/activate
pip3 install west
west init -l .
west update
west zephyr-export
pip3 install -r scripts/requirements-base.txt

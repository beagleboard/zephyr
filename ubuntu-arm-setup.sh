#!/bin/bash -ex
ENVNAME=zephyr-beagle-cc1352-env
sudo apt update
sudo apt install --no-install-recommends -y \
    cmake \
    gcc python3-dev \
    device-tree-compiler \
    gperf \
    ccache dfu-util \
    libsdl2-dev \
    libxml2-dev libxslt1-dev libssl-dev libturbojpeg0-dev libmagic1 \
    libtool-bin autoconf automake libusb-1.0-0-dev \
    python3-tk python3-virtualenv
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.15.1/zephyr-sdk-0.15.1_linux-aarch64_minimal.tar.gz
tar xf zephyr-sdk-0.15.1_linux-aarch64_minimal.tar.gz
rm zephyr-sdk-0.15.1_linux-aarch64_minimal.tar.gz
./zephyr-sdk-0.15.1/setup.sh -t arm-zephyr-eabi -c
python3 -m virtualenv $ENVNAME
echo "export ZEPHYR_TOOLCHAIN_VARIANT=zephyr" >> ./$ENVNAME/bin/activate
echo "export ZEPHYR_SDK_INSTALL_DIR=$(pwd)/zephyr-sdk-0.15.1" >> ./$ENVNAME/bin/activate
echo "export ZEPHYR_BASE=$(pwd)/$ENVNAME/zephyr" >> ./$ENVNAME/bin/activate
echo 'export PATH=$ZEPHYR_BASE/scripts:$PATH' >> ./$ENVNAME/bin/activate
source $ENVNAME/bin/activate
pip3 install west
west init -l .
west update
west zephyr-export
pip3 install -r scripts/requirements-base.txt

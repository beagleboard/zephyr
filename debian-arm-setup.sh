#!/bin/bash -ex
sudo apt update
sudo apt install --no-install-recommends -y \
    gperf \
    ccache dfu-util \
    libsdl2-dev \
    libxml2-dev libxslt1-dev libssl-dev libjpeg62-turbo-dev libmagic1 \
    libtool-bin autoconf automake libusb-1.0-0-dev \
    python3-tk python3-virtualenv
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.15.1/zephyr-sdk-0.15.1_linux-aarch64_minimal.tar.gz
tar xf zephyr-sdk-0.15.1_linux-aarch64_minimal.tar.gz
rm zephyr-sdk-0.15.1_linux-aarch64_minimal.tar.gz
./zephyr-sdk-0.15.1/setup.sh -t arm-zephyr-eabi -c
west init -l .
cd $HOME/zephyr-beagle-cc1352-sdk
python3 -m virtualenv zephyr-beagle-cc1352-env
echo "export ZEPHYR_TOOLCHAIN_VARIANT=zephyr" >> $HOME/zephyr-beagle-cc1352-sdk/zephyr-beagle-cc1352-env/bin/activate
echo "export ZEPHYR_SDK_INSTALL_DIR=$HOME/zephyr-sdk-0.15.1" >> $HOME/zephyr-beagle-cc1352-sdk/zephyr-beagle-cc1352-env/bin/activate
echo "export ZEPHYR_BASE=$HOME/zephyr-beagle-cc1352-sdk/zephyr" >> $HOME/zephyr-beagle-cc1352-sdk/zephyr-beagle-cc1352-env/bin/activate
echo 'export PATH=$HOME/zephyr-beagle-cc1352-sdk/zephyr/scripts:$PATH' >> $HOME/zephyr-beagle-cc1352-sdk/zephyr-beagle-cc1352-env/bin/activate
source $HOME/zephyr-beagle-cc1352-sdk/zephyr-beagle-cc1352-env/bin/activate
west update
west zephyr-export
pip3 install -r zephyr/scripts/requirements-base.txt

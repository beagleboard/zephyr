#!/bin/sh
# Source this script
ENV_NAME=zephyr
SDK_PATH=$HOME/zephyr_sdk
if [ ! -e $ENV_NAME ]; then
  python3 -m venv $ENV_NAME
  echo "export ZEPHYR_TOOLCHAIN_VARIANT=$ENV_NAME" >> ./zephyr/bin/activate
  echo "export ZEPHYR_SDK_INSTALL_DIR=$SDK_PATHk" >> ./zephyr/bin/activate
  echo "export ZEPHYR_BASE=$(pwd)" >> ./zephyr/bin/activate
fi
source zephyr/bin/activate


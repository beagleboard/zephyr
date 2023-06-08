#!/bin/bash
# example: ./ci-prepend-mcuboot.sh freedom mcumgr
mkdir -p build/$1/$2-w-boot/zephyr
cp temp/$1/mcuboot/zephyr/zephyr.bin build/$1/$2-w-boot/zephyr/
dd conv=notrunc bs=1024 seek=36 if=temp/$1/$2/zephyr/zephyr.signed.bin of=build/$1/$2-w-boot/zephyr/zephyr.bin

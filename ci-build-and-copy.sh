#!/bin/bash
# ./ci-build-and-copy.sh <build_dir> <build_subdir> <board_name> <application_path> [<bootloader_to_prepend>]
# example: ./ci-build-and-copy.sh play blinky beagleplay samples/basic/blinky mcuboot
west build -d temp/$1/$2 -b $3 $4
mkdir -p build/$1/$2/zephyr
cp temp/$1/$2/zephyr/zephyr.bin build/$1/$2/zephyr/

# Perform prepend
if [ "$5" != "" ]; then
  mkdir -p build/$1/$2-w-boot/zephyr
  cp temp/$1/$5/zephyr/zephyr.bin build/$1/$2-w-boot/zephyr/
  dd conv=notrunc bs=1024 seek=36 if=temp/$1/$2/zephyr/zephyr.signed.bin of=build/$1/$2-w-boot/zephyr/zephyr.bin
fi

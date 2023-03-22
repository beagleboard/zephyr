#!/bin/bash
# example: ./ci-build-and-copy.sh play blinky beagleplay samples/basic/blinky
west build -d temp/$1/$2 -b $3 $4
mkdir -p build/$1/$2/zephyr
cp temp/$1/$2/zephyr/zephyr.bin build/$1/$2/zephyr/

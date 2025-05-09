# BeagleBoard.org Zephyr Tree

This repository is a BeagleBoard.org specific upstream of ZephyrRTOS, intended for experimentation
before PRs are merged upstream. The support here is on a best effort basis. If the board support has
already been merged upstream, it is likely a better idea to use the upstream Zephyr tree.

# Getting Started

Then instructions are mostly the same as [Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

However, to use this tree directly from west, use the following command:

```
west init -m https://github.com/beagleboard/zephyr.git --mr zephyr-4.1.0 --mf west_beagle.yml
```

The above command will clone only the dependencies required for use with BeagleBoard.org boards.

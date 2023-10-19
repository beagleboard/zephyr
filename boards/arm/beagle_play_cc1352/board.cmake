# Copyright (c) 2020 Erik Larson
# Copyright (c) 2023 Jason Kridner, BeagleBoard.org Foundation
#
# SPDX-License-Identifier: Apache-2.0

# Copy https://git.beagleboard.org/beagleconnect/cc1352-flasher/-/raw/3.0a1/cc1352-flasher.py here
# Install python gpiod library
# Be sure to disable the bcfserial driver because it will capture /dev/ttyS4

board_set_flasher_ifnset(misc-flasher)
board_finalize_runner_args(misc-flasher "$ENV{ZEPHYR_BASE}/boards/arm/beagle_play_cc1352/cc1352-flasher.py --play")

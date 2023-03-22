# Copyright (c) 2020 Erik Larson
# Copyright (c) 2022 Jason Kridner, BeagleBoard.org Foundation
#
# SPDX-License-Identifier: Apache-2.0

# Copy https://git.beagleboard.org/beagleconnect/cc1352-flasher/-/raw/3.0a2/cc1352-flasher.py here
# Install python pyserial library

board_set_flasher_ifnset(misc-flasher)
board_finalize_runner_args(misc-flasher "$ENV{ZEPHYR_BASE}/boards/arm/beagle_bcf/cc1352-flasher.py --bcf")

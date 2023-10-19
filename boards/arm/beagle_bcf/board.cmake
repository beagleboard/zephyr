# Copyright (c) 2020 Erik Larson
# Copyright (c) 2022 Jason Kridner, BeagleBoard.org Foundation
#
# SPDX-License-Identifier: Apache-2.0

# Download cc1352-flasher.py from https://git.beagleboard.org/beagleconnect/cc1352-flasher/-/tags/3.0a0

board_set_flasher_ifnset(misc-flasher)
board_finalize_runner_args(misc-flasher $ENV{ZEPHYR_BASE}/boards/arm/beagle_bcf/cc1352-flasher.py --bcf)

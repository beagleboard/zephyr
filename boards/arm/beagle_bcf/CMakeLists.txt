# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources(
    board_antenna.c
    )

option (CONFIG_SOC_CC1352P7 "SoC is CC1352P7" OFF)
option (CONFIG_SOC_CC1352P "SoC is CC1352P1" OFF)
if (CONFIG_SOC_CC1352P7)
    zephyr_library_compile_definitions("DeviceFamily_CC13X2X7")
endif ()
if (CONFIG_SOC_CC1352P)
    zephyr_library_compile_definitions("DeviceFamily_CC13X2")
endif ()

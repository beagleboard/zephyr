.. _beagleconnect_freedom:

BeagleConnect Freedom
#####################

Overview
********

BeagleBoard.org BeagleConnect Freedom is a wireless
Internet of Things board based on the SimpleLink multi-Standard CC1352P wireless MCU.


.. figure:: img/beagleconnect_freedom.png
   :width: 400px
   :align: center
   :alt: BeagleBoard.org BeagleConnect Freedom

   BeagleBoard.org BeagleConnect Freedom

Hardware
********
BeagleBoard.org BeagleConnect Freedom board features the CC1352P wireless microcontroller.
The BeagleConnect Freedom is the first available BeagleConnect solution consisting
of a board and a case which ships programmed and ready to be used.

BeagleConnect Freedom board runs the Zephyr RTOS and has mikroBUS ports along 
with BLE and Sub-GHz radios on it.

The CC1352P wireless MCU has a 48 MHz Arm Cortex-M4F SoC and a Bluetooth Low Energy and IEEE 802.15.4.


Supported Features
==================

The CC1352P LaunchPad board configuration supports the following hardware
features:

+-----------+------------+----------------------+
| Interface | Controller | Driver/Component     |
+===========+============+======================+
| GPIO      | on-chip    | gpio                 |
+-----------+------------+----------------------+
| PINMUX    | on-chip    | pinmux               |
+-----------+------------+----------------------+
| UART      | on-chip    | serial               |
+-----------+------------+----------------------+
| I2C       | on-chip    | i2c                  |
+-----------+------------+----------------------+
| SPI       | on-chip    | spi                  |
+-----------+------------+----------------------+

Other hardware features are not supported by the Zephyr kernel.

Connections and IOs
===================

+-------+--------------+-------------------------------------+
| Pin   | Function     | Usage                               |
+=======+==============+=====================================+
| DIO5  | RST_MB2      | Reset mikroBUS port 2               |
+-------+--------------+-------------------------------------+
| DIO6  | RST_MB1      | Reset mikroBUS port 1               |
+-------+--------------+-------------------------------------+
| DIO7  | INT_SENSOR   | On-board sensor interrupts          |
+-------+--------------+-------------------------------------+
| DIO8  | FLASH_CS     | SPI flash chip-select               |
+-------+--------------+-------------------------------------+
| DIO9  | SDO          | SPI serial data output              |
+-------+--------------+-------------------------------------+
| DIO10 | SCK          | SPI serial clock                    |
+-------+--------------+-------------------------------------+
| DIO11 | SDI          | SPI serial data input               |
+-------+--------------+-------------------------------------+
| DIO12 |              | UART RXD mikroBUS port 1 or MSP430  |
+-------+--------------+-------------------------------------+
| DIO13 |              | UART TXD mikroBUS port 1 or MSP430  |
+-------+--------------+-------------------------------------+
| DIO14 | I2C_CTRL     | Enable on-board sensor I2C bus      |
+-------+--------------+-------------------------------------+
| DIO15 | USER_BOOT    | BOOT button status                  |
+-------+--------------+-------------------------------------+
| DIO16 | INT_MB1      | INTERRUPT PIN on mikroBUS port 1    |
+-------+--------------+-------------------------------------+
| DIO17 | PWM_MB1      | PWM PIN on mikroBUS port 1          |
+-------+--------------+-------------------------------------+
| DIO18 | 2.4G         | Enable 2.4GHz on external antenna   |
+-------+--------------+-------------------------------------+
| DIO19 | PWM_MB2      | PWM PIN on mikroBUS port 2          |
+-------+--------------+-------------------------------------+
| DIO20 | INT_MB2      | INTERRUPT PIN on mikroBUS port 2    |
+-------+--------------+-------------------------------------+
| DIO21 | TX_MB2_RX    | UART RXD on mikroBUS port 2         |
+-------+--------------+-------------------------------------+
| DIO22 | RX_MB2_TX    | UART TXD on mikroBUS port 2         |
+-------+--------------+-------------------------------------+
| DIO23 | AN_MB1       | ANALOG PIN on mikroBUS port 1       |
+-------+--------------+-------------------------------------+
| DIO24 | AN_MB2       | ANALOG PIN on mikroBUS port 2       |
+-------+--------------+-------------------------------------+
| DIO25 | SCL          | I2C SCL                             |
+-------+--------------+-------------------------------------+
| DIO26 | SDA          | I2C SDA                             |
+-------+--------------+-------------------------------------+
| DIO27 | CS_MB2       | SPI CS on microBUS port 2           |
+-------+--------------+-------------------------------------+
| DIO28 | CS_MB1       | SPI CS on microBUS port 1           |
+-------+--------------+-------------------------------------+
| DIO29 | REF_SW_CTRL1 | TBD                                 |
+-------+--------------+-------------------------------------+
| DIO30 | REF_SW_CTRL2 | TBD                                 |
+-------+--------------+-------------------------------------+

References
**********


BeagleBoard.org BeagleConnect Freedom reference repository:
  https://beagleconnect.org

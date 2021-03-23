/* hm3301.c - Driver for HM3301 particulate matter sensor
 */

/*
 * Copyright (c) 2021 Jason Kridner
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT seeed_hm3301

#include <drivers/i2c.h>
#include <init.h>
#include <kernel.h>
#include <sys/byteorder.h>
#include <sys/__assert.h>
#include <drivers/sensor.h>
#include <net/net_ip.h>
#include <stdio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(hm3301, CONFIG_SENSOR_LOG_LEVEL);

#include <device.h>
#include <zephyr/types.h>
struct hm3301_data {
	const struct device *i2c_ctrl;
	uint16_t i2c_addr;

	uint16_t pm1p0_std;
	uint16_t pm2p5_std;
	uint16_t pm10_std;

	uint16_t pm1p0_atm;
	uint16_t pm2p5_atm;
	uint16_t pm10_atm;
};

static uint8_t hm3301_validateChecksum(uint8_t *data, size_t size)
{
	uint8_t sum = 0;
	for (uint8_t i = 0; i < size - 1; i++) {
		sum += data[i];
	}

	if (data[size - 1] != (sum & 0xFF)) {
		return -EBADMSG;
	}

	return 0;
}

static int hm3301_do_read(struct hm3301_data *data, uint8_t *rb, size_t size)
{
	int ret;
	uint8_t wb[] = { 0x88, 29 };

	if (size != 29) {
		return -ENOTSUP;
	}

	ret = i2c_write_read(data->i2c_ctrl, data->i2c_addr, wb, 1, rb, size);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static int hm3301_sample_fetch(const struct device *dev,
			       enum sensor_channel chan)
{
	int ret;
	struct hm3301_data *data = dev->data;
	uint8_t rb[29];

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	ret = hm3301_do_read(data, rb, 29);
	if (ret < 0) {
		LOG_ERR("Read error: %d", ret);
		return ret;
	}

	ret = hm3301_validateChecksum(rb, 29);
	if (ret < 0) {
		LOG_ERR("Checksum error: %d", ret);
		return ret;
	}

	data->pm1p0_std = sys_be16_to_cpu(*(uint16_t *)(rb+4));
	data->pm2p5_std = sys_be16_to_cpu(*(uint16_t *)(rb+6));
	data->pm10_std = sys_be16_to_cpu(*(uint16_t *)(rb+8));

	data->pm1p0_atm = sys_be16_to_cpu(*(uint16_t *)(rb+10));
	data->pm2p5_atm = sys_be16_to_cpu(*(uint16_t *)(rb+12));
	data->pm10_atm = sys_be16_to_cpu(*(uint16_t *)(rb+14));

	return 0;
}

static int hm3301_channel_get(const struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct hm3301_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_PM_1_0:
		/*
		 * 1.0 micro-meters particulate matter in ug/m^3
		 */
		val->val1 = (uint32_t)data->pm1p0_atm;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_PM_2_5:
		/*
		 * 2.5 micro-meters particulate matter in ug/m^3
		 */
		val->val1 = (uint32_t)data->pm2p5_atm;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_PM_10:
		/*
		 * 10 micro-meters particulate matter in ug/m^3
		 */
		val->val1 = (uint32_t)data->pm10_atm;
		val->val2 = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int hm3301_init(const struct device *dev)
{
	int err;
	struct hm3301_data *data = dev->data;

	data->i2c_ctrl = device_get_binding(
		DT_INST_BUS_LABEL(0));
	if (!data->i2c_ctrl) {
		LOG_ERR("I2C controller not found: %s",
			    DT_INST_BUS_LABEL(0));
		return -EINVAL;
	}

	data->i2c_addr = DT_INST_REG_ADDR(0);

	// Try 500ms delay at the start
	k_msleep(500);

	err = hm3301_sample_fetch(dev, SENSOR_CHAN_ALL);
	if (err < 0) {
		LOG_ERR("Initial read error: %d", err);
		goto recover;
	}

	return 0;

recover:
	i2c_recover_bus(data->i2c_ctrl);
	return -EINVAL;
}

static const struct sensor_driver_api hm3301_api_funcs = {
	.sample_fetch = hm3301_sample_fetch,
	.channel_get = hm3301_channel_get,
};

static struct hm3301_data hm3301_data;

DEVICE_DEFINE(hm3301, DT_INST_LABEL(0), hm3301_init,
	      NULL, &hm3301_data, NULL, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
	      &hm3301_api_funcs);

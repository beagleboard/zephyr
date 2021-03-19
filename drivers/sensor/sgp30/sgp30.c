/* sgp30.c - Driver for Sensirion's SGP30 temperature, pressure,
 * humidity and gas sensor
 *
 * https://www.sensirion.com/en/environmental-sensors/gas-sensors/sgp30/
 */

/*
 * Copyright (c) 2021 Jason Kridner
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sensirion_sgp30

#include <drivers/i2c.h>
#include <init.h>
#include <kernel.h>
#include <sys/byteorder.h>
#include <sys/__assert.h>
#include <drivers/sensor.h>
#include <net/net_ip.h>
#include <stdio.h>
#include <math.h>

#include <device.h>
#include <zephyr/types.h>

#define SGP30_FEATURESET                0x0020
#define SGP30_CRC8_POLYNOMIAL           0x31
#define SGP30_CRC8_INIT                 0xFF
#define SGP30_WORD_LEN                  0x31

struct sgp30_data {
	struct k_work sample_worker;
	struct k_timer sample_timer;
	const struct device *i2c_ctrl;
	uint16_t i2c_addr;
	uint16_t serialid[3];
	uint16_t featureset;

	uint16_t TVOC;
	uint16_t eCO2;
	uint16_t rawH2;
	uint16_t rawEthanol;

	uint16_t absoluteHumidity;
};

#include <logging/log.h>
LOG_MODULE_REGISTER(sgp30, CONFIG_SENSOR_LOG_LEVEL);

/* From Adafruit_SGP30 library */
static uint8_t sgp30_generateCRC(uint8_t *data)
{
	uint8_t crc = SGP30_CRC8_INIT;

	for (uint8_t i = 0; i < 2; i++) {
		crc ^= data[i];
		for (uint8_t b = 0; b < 8; b++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ SGP30_CRC8_POLYNOMIAL;
			else
				crc <<= 1;
		}
	}

	return crc;
}

static int sgp30_cmd(struct sgp30_data *data, uint16_t command, uint16_t delay_ms,
		      uint16_t *reply_buf, size_t size)
{
	int ret;
	size_t i;
	uint8_t crc;
	uint32_t rb_size = (uint32_t)(size*2+size);
	uint8_t read_buf[9];
	uint16_t command_be = htons(command); /* make sure it is big endian */
	uint8_t *cmdp = (uint8_t *)&command_be;

	LOG_DBG("Writing 0x%02x 0x%02x to 0x%02x", cmdp[0], cmdp[1], data->i2c_addr);
	ret = i2c_write(data->i2c_ctrl, cmdp, 2, data->i2c_addr);
	if (ret < 0) {
		LOG_ERR("Failed to send command %04x: %d", command_be, ret);
		return ret;
	}

	LOG_DBG("Sleeping %d ms", delay_ms);
	if (delay_ms > 0) {
		k_msleep((uint32_t)delay_ms);
	}

	if (reply_buf == NULL || size == 0) {
		return 0;
	}

	if (size > 3) {
		return -EOVERFLOW;
	}

	LOG_DBG("Reading %d bytes from %d", rb_size, data->i2c_addr);
	ret = i2c_read(data->i2c_ctrl, read_buf, rb_size,
		       	data->i2c_addr);
	if (ret < 0) {
		LOG_ERR("Failed to read: %d", ret);
		return ret;
	}

	for (i = 0; i < size; i++) {
		crc = sgp30_generateCRC(read_buf + i * 3);
		if (crc != read_buf[i * 3 + 2]) {
			LOG_ERR("CRC error: %d != %d", read_buf[i * 3 + 2], crc);
			return -EBADMSG;
		}
		reply_buf[i] = ntohs(*(uint16_t *)(read_buf + i * 3));
	}

	return 0;
}

static int sgp30_write(struct sgp30_data *data, uint16_t command,
		        uint16_t *val, size_t size)
{
	int ret;
	size_t i;
	uint32_t wb_size = 2+size*3;
	uint8_t write_buf[9];
	uint16_t be;
	uint8_t *bep;
       
	be = htons(command); /* make sure it is big endian */
	bep = (uint8_t *)&be;
	memcpy(write_buf, bep, 2);

	LOG_DBG("Writing command 0x%04x", command);
	for (i = 0; i < size; i++) {
		be = htons(val[i]);
		bep = (uint8_t *)&be;
		memcpy(&write_buf[2+i*3], bep, 2);
		write_buf[4+i*3] = sgp30_generateCRC(bep);
		LOG_DBG("write_buf[%d] = 0x%04x", i, val[i]);
		LOG_DBG("crc(write_buf[%d]) = 0x%02x", i, write_buf[4+i*3]);
	}

	LOG_DBG("Writing %d bytes to 0x%02x", wb_size, data->i2c_addr);
	ret = i2c_write(data->i2c_ctrl, write_buf, wb_size, data->i2c_addr);
	if (ret < 0) {
		LOG_ERR("Write error: %d", ret);
		return ret;
	}

	return 0;
}

static int sgp30_sample_fetch(const struct device *dev,
			      enum sensor_channel chan)
{
	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	return 0;
}

static int sgp30_sample_read(struct sgp30_data *data)
{
	uint16_t reply[3]; /* Size must include extra bytes for CRC */
	int ret;

	ret = sgp30_cmd(data, 0x2008, 12, reply, 2);
	if (ret < 0) {
		return ret;
	}

	data->TVOC = reply[1];
	data->eCO2 = reply[0];

	ret = sgp30_cmd(data, 0x2050, 25, reply, 2);
	if (ret < 0) {
		return ret;
	}

	data->rawEthanol = reply[1];
	data->rawH2 = reply[0];
	return 0;
}

static int sgp30_channel_get(const struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct sgp30_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_VOC:
		/*
		 * TVOC in PPB
		 */
		val->val1 = (uint32_t)data->TVOC;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_CO2:
		/*
		 * CO2 in PPM
		 */
		val->val1 = (uint32_t)data->eCO2;
		val->val2 = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sgp30_chip_init(const struct device *dev)
{
	struct sgp30_data *data = (struct sgp30_data *)dev->data;
	int err;

	/* Clear absoluteHumidity */
	data->absoluteHumidity = 0;

	/* Get Serial ID */
	LOG_DBG("Fetching Serial ID");
	err = sgp30_cmd(data, 0x3682, 1, data->serialid, 3);
	if (err < 0) {
		LOG_ERR("Failed to fetch Serial ID: %d", err);
		return err;
	}
	LOG_DBG("Serial ID: %04x%04x%04x", data->serialid[0],
			data->serialid[1], data->serialid[2]);

	/* Check FEATURESET */
	LOG_DBG("Fetching FEATURESET");
	err = sgp30_cmd(data, 0x202F, 10, &data->featureset, 1);
	if (err < 0) {
		LOG_ERR("Failed to fetch FEATURESET: %d", err);
		return err;
	}
	LOG_DBG("FEATURESET: %04x", data->featureset);
	if ((data->featureset & 0xF0) != SGP30_FEATURESET) {
		LOG_ERR("Bad FEATURESET: %d", err);
		return -ENOTSUP;
	}

	/* Start IAQ algorithm */
	LOG_DBG("Starting IAQ algorithm");
	err = sgp30_cmd(data, 0x2003, 0, NULL, 0);
	if (err < 0) {
		return err;
	}

	return 0;
}

static void sgp30_sample_worker(struct k_work *work)
{
	int err;
	struct sgp30_data *data = 
		CONTAINER_OF(work, struct sgp30_data, sample_worker);

	err = sgp30_sample_read(data);
	if (err < 0) {
		LOG_ERR("sgp30_sample_read error: %d", err);
	}
}

static void sgp30_timer(struct k_timer *timer)
{
	struct sgp30_data *data = 
		CONTAINER_OF(timer, struct sgp30_data, sample_timer);
	k_work_submit(&data->sample_worker);
}

static int sgp30_init(const struct device *dev)
{
	struct sgp30_data *data = dev->data;

	data->i2c_ctrl = device_get_binding(
		DT_INST_BUS_LABEL(0));
	if (!data->i2c_ctrl) {
		LOG_ERR("I2C controller not found: %s",
			    DT_INST_BUS_LABEL(0));
		return -EINVAL;
	}

	//uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER;
	//if (i2c_configure(data->i2c_ctrl, i2c_cfg))
	//{
	//	LOG_ERR("I2C config failed");
	//	goto recover;
	//}

	data->i2c_addr = DT_INST_REG_ADDR(0);

	if (sgp30_chip_init(dev) < 0) {
		LOG_ERR("SGP30 init failed");
		goto recover;
	}

	k_work_init(&data->sample_worker, sgp30_sample_worker);
	k_timer_init(&data->sample_timer, sgp30_timer, NULL);
	k_timer_start(&data->sample_timer, K_SECONDS(1), K_SECONDS(1));

	return 0;

recover:
	i2c_recover_bus(data->i2c_ctrl);
	return -EINVAL;
}

static int sgp30_attr_set(const struct device *dev, enum sensor_channel chan,
			   enum sensor_attribute attr,
			   const struct sensor_value *val)
{
	struct sgp30_data *data = dev->data;
	uint16_t ah_fixedpt; /* Absolute humidity in 256ths of g*m^-3 */
	double ah, rh; /* Absolute and relative humidities */
	double t; /* Temperature in C */

	switch(attr) {
	case SENSOR_ATTR_CALIB_TARGET:
		LOG_DBG("Calibrating with %d RH and %d C", val[0].val1, val[1].val1);
		rh = sensor_value_to_double((struct sensor_value *)&val[0]);
		LOG_DBG("rh = %f", rh);
		t = sensor_value_to_double((struct sensor_value *)&val[1]);
		LOG_DBG("t = %f", t);
		ah = 216.7*(rh*0.06112*exp((17.62*t)/(243.12*t)))/(273.15+t);
		ah_fixedpt = (uint16_t)(ah*256.0);
		sgp30_write(data, 0x2061, &ah_fixedpt, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct sensor_driver_api sgp30_api_funcs = {
	.sample_fetch = sgp30_sample_fetch,
	.channel_get = sgp30_channel_get,
	.attr_set = sgp30_attr_set,
};

static struct sgp30_data sgp30_data;

DEVICE_DEFINE(sgp30, DT_INST_LABEL(0), sgp30_init, NULL, &sgp30_data,
		    NULL, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &sgp30_api_funcs);

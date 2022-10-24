/*
 * Copyright (c) 2021 Jason Kridner, BeagleBoard.org Foundation
 * Copyright (c) 2020 Innoseis BV
 *
 * Based on i2c_tca9656a.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_ts5a2066

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ts5a2066, CONFIG_I2C_LOG_LEVEL);

struct ts5a2066_config {
	const struct device * bus;
	const struct device * gpiodev;
	const gpio_pin_t gpiopin;
	const gpio_flags_t gpioflags;
};

struct ts5a2066_data {
	struct k_mutex lock;
};


int ts5a2066_configure(const struct device *dev, uint32_t dev_config)
{
	const struct ts5a2066_config * config = dev->config;

	return i2c_configure(config->bus, dev_config);
}

static int ts5a2066_enable(const struct device *dev)
{
	int res = 0;
	const struct ts5a2066_config *config = dev->config;

	//LOG_DBG("Enabling TS5A2066 using GPIO %d", config->gpiopin);
	//res = gpio_pin_set_raw(config->gpiodev, config->gpiopin, 1);
	res = gpio_pin_configure(config->gpiodev, config->gpiopin, GPIO_OUTPUT_HIGH);

	return res;
}

static int ts5a2066_disable(const struct device *dev)
{
	int res = 0;
	const struct ts5a2066_config *config = dev->config;

	//LOG_DBG("Disabling TS5A2066 using GPIO %d", config->gpiopin);
	//res = gpio_pin_set_raw(config->gpiodev, config->gpiopin, 0);
	res = gpio_pin_configure(config->gpiodev, config->gpiopin, GPIO_OUTPUT_LOW);

	return res;
}
static int ts5a2066_transfer(const struct device *dev,
			     struct i2c_msg *msgs,
			     uint8_t num_msgs,
			     uint16_t addr)
{
	struct ts5a2066_data *data = dev->data;
	const struct ts5a2066_config *config = dev->config;
	int res;

	res = k_mutex_lock(&data->lock, K_MSEC(5000));
	if (res != 0) {
		return res;
	}

	ts5a2066_enable(dev);
	k_busy_wait(100);
	res = i2c_transfer(config->bus, msgs, num_msgs, addr);
	k_busy_wait(100);
	ts5a2066_disable(dev);

	k_mutex_unlock(&data->lock);
	return res;
}

const struct i2c_driver_api ts5a2066_api_funcs = {
	.configure = ts5a2066_configure,
	.transfer = ts5a2066_transfer,
};

static int ts5a2066_init(const struct device *dev)
{
	const struct ts5a2066_config * config = dev->config;

	gpio_pin_configure(config->gpiodev, config->gpiopin, GPIO_OUTPUT_LOW);

	return 0;
}

#define	DEFINE_TS5A2066(inst)						\
									\
static struct ts5a2066_data ts5a2066_dev_data_##inst;			\
									\
static const struct ts5a2066_config ts5a2066_dev_cfg_##inst = {		\
	.bus		= DEVICE_DT_GET(DT_PHANDLE(DT_INST(inst, DT_DRV_COMPAT), controller)),	\
	.gpiodev	= DEVICE_DT_GET(DT_PHANDLE(DT_INST(inst, DT_DRV_COMPAT), gpios)),	\
	.gpiopin	= DT_INST_GPIO_PIN(inst, gpios),		\
	.gpioflags	= DT_INST_GPIO_FLAGS(inst, gpios),		\
};									\
									\
DEVICE_DT_INST_DEFINE(inst,						\
	    ts5a2066_init,						\
	    device_pm_control_nop,					\
	    &ts5a2066_dev_data_##inst,					\
	    &ts5a2066_dev_cfg_##inst,					\
	    POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &ts5a2066_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_TS5A2066)

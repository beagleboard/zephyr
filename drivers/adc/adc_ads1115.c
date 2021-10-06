/*
 * Copyright (c) 2021 Jackychen
 * Copyright (c) 2021 Jason Kridner, BeagleBoard.org Foundation
 *
 * Based partially on adc_mcp320x.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_ads1115

#include <drivers/adc.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <kernel.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <sys/util.h>
#include <zephyr.h>
#include <device.h>
#include <math.h>

LOG_MODULE_REGISTER(ads1115, CONFIG_ADC_LOG_LEVEL);
//LOG_MODULE_REGISTER(ads1115, LOG_LEVEL_DBG);

/* TODO: this should change to use the interrupt line */
#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

#define ADS1115_RESOLUTION 16U

#define ADS1115_REG_CONVERSION		0x00
#define ADS1115_REG_CONFIG		0x01
#define ADS1115_REG_LO_TH		0x02
#define ADS1115_REG_HI_TH		0x03

#define ADS1115_NUM_CHANNELS	4

enum average_method {
	RMS = 0,
	MEAN = 1,
};

#define _B(n)		( 1<<(n) )
#define _M(l)		( _B(l)-1 )
#define _BM(o, l)	( _M(l)<<(o) )
#define _BP(x, o, l)	( ((x)&_M(l)) << (o) )
#define _BG(x, o, l)	( ((x)>>(o)) & _M(l) )
#define _BS(y, x, o, l) \
	( y = ((y) &~ _BM(o,l)) | _BP(x,o,l) )

#define ADS1115_CFG_OS_GET() _BG(data->config_reg, 15, 1)
#define ADS1115_CFG_OS_SET(x) _BS(data->config_reg, x, 15, 1)
#define ADS1115_CFG_MUX_GET() _BG(data->config_reg, 12, 3)
#define ADS1115_CFG_MUX_SET(x) _BS(data->config_reg, x, 12, 3)
#define ADS1115_CFG_PGA_GET() _BG(data->config_reg, 9, 3)
#define ADS1115_CFG_PGA_SET(x) _BS(data->config_reg, x, 9, 3)
#define ADS1115_CFG_MODE_GET() _BG(data->config_reg, 8, 1)
#define ADS1115_CFG_MODE_SET(x) _BS(data->config_reg, x, 8, 1)
#define ADS1115_CFG_DR_GET() _BG(data->config_reg, 5, 3)
#define ADS1115_CFG_DR_SET(x) _BS(data->config_reg, x, 5, 3)
#define ADS1115_CFG_CM_GET() _BG(data->config_reg, 4, 1)
#define ADS1115_CFG_CM_SET(x) _BS(data->config_reg, x, 4, 1)
#define ADS1115_CFG_CP_GET() _BG(data->config_reg, 3, 1)
#define ADS1115_CFG_CP_SET(x) _BS(data->config_reg, x, 3, 1)
#define ADS1115_CFG_CL_GET() _BG(data->config_reg, 2, 1)
#define ADS1115_CFG_CL_SET(x) _BS(data->config_reg, x, 2, 1)
#define ADS1115_CFG_CQ_GET() _BG(data->config_reg, 0, 2)
#define ADS1115_CFG_CQ_SET(x) _BS(data->config_reg, x, 0, 2)

struct ads1115_config {
	struct i2c_dt_spec		bus;
	struct gpio_dt_spec		int_gpio;
	bool				continuous_mode;
	enum average_method		avg_method;
	uint16_t			threshold;
};

struct ads1115_data {
	const struct ads1115_config *	cfg;
	struct adc_context		ctx;
	const struct device *		dev;
	uint16_t *			buffer;
	uint16_t *			repeat_buffer;
	struct k_thread			thread;
	struct k_sem			sem;
	uint16_t			config_reg;
	uint8_t				seq_channels;
	uint8_t				differential;
	bool				active;
	uint8_t				active_channel;
	uint16_t			oversampling;

	struct gpio_callback		gpio_cb;

	K_KERNEL_STACK_MEMBER(stack,
			CONFIG_ADC_ADS1115_ACQUISITION_THREAD_STACK_SIZE);
};

static int ads1115_init(const struct device *dev);
static int ads1115_channel_setup(const struct device *dev,
				 const struct adc_channel_cfg *channel_cfg);
static int ads1115_read(const struct device *dev,
			const struct adc_sequence *sequence);
static int ads1115_read_async(const struct device *dev,
			      const struct adc_sequence *sequence,
			      struct k_poll_signal *async);

static int ads1115_reg_write(struct ads1115_data * data, uint8_t reg, uint16_t * val);
static int ads1115_reg_read(struct ads1115_data * data, uint8_t reg, uint16_t * val);
static int ads1115_chan_change_with_start_conversion(struct ads1115_data * data, uint8_t channel);
static int ads1115_once_conversion_status_get(struct ads1115_data * data, int * status);

static const struct adc_driver_api ads1115_api_funcs = {
	.channel_setup = ads1115_channel_setup,
	.read = ads1115_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async = ads1115_read_async,
#endif
	.ref_internal = 4096,
};

static int ads1115_channel_setup(const struct device * dev,
				 const struct adc_channel_cfg * channel_cfg)
{
	/* const struct ads1115_config * config = dev->config; */

	/* TODO */
	if (channel_cfg->gain != ADC_GAIN_1) {
		LOG_ERR("unsupported channel gain '%d'", channel_cfg->gain);
		return -ENOTSUP;
	}

	if (channel_cfg->reference != ADC_REF_INTERNAL) {
		LOG_ERR("unsupported channel reference '%d'",
			channel_cfg->reference);
		return -ENOTSUP;
	}

	/* TODO */
	if (channel_cfg->acquisition_time != ADC_ACQ_TIME_DEFAULT) {
		LOG_ERR("unsupported acquisition_time '%d'",
			channel_cfg->acquisition_time);
		return -ENOTSUP;
	}

	/* TODO */
	/* ADS1115 supports 2 differential inputs -- not yet implemented */
	if (channel_cfg->differential != 0) {
		LOG_ERR("unsupported differential '%d'",
			channel_cfg->differential);
		return -ENOTSUP;
	}

	if (channel_cfg->channel_id >= ADS1115_NUM_CHANNELS) {
		LOG_ERR("unsupported channel id '%d'", channel_cfg->channel_id);
		return -ENOTSUP;
	}

	return 0;
}

static int ads1115_validate_buffer_size(const struct device * dev,
					const struct adc_sequence * sequence)
{
	uint8_t channels = 0;
	size_t needed;
	uint32_t mask;

	for (mask = BIT(ADS1115_NUM_CHANNELS - 1); mask != 0; mask >>= 1) {
		if (mask & sequence->channels) {
			channels++;
		}
	}

	needed = channels * sizeof(uint16_t);
	if (sequence->options) {
		needed *= (1 + sequence->options->extra_samplings);
	}

	if (sequence->buffer_size < needed) {
		return -ENOMEM;
	}

	return 0;
}

static int ads1115_start_read(const struct device * dev,
			      const struct adc_sequence * sequence)
{
	struct ads1115_data * data = dev->data;
	int err;

	if (sequence->resolution != 16) {
		LOG_ERR("unsupported resolution %d", sequence->resolution);
		return -ENOTSUP;
	}

	if (find_msb_set(sequence->channels) > ADS1115_NUM_CHANNELS) {
		LOG_ERR("unsupported channels in mask: 0x%08x",
			sequence->channels);
		return -ENOTSUP;
	}

	err = ads1115_validate_buffer_size(dev, sequence);
	if (err) {
		LOG_ERR("buffer size too small");
		return err;
	}

	data->buffer = sequence->buffer;
	data->oversampling = sequence->oversampling;
	adc_context_start_read(&data->ctx, sequence);

	return adc_context_wait_for_completion(&data->ctx);
}

static int ads1115_read_async(const struct device * dev,
			      const struct adc_sequence * sequence,
			      struct k_poll_signal * async)
{
	struct ads1115_data * data = dev->data;
	int err;

	adc_context_lock(&data->ctx, async ? true : false, async);
	err = ads1115_start_read(dev, sequence);
	adc_context_release(&data->ctx, err);

	return err;
}

static int ads1115_read(const struct device * dev,
			const struct adc_sequence * sequence)
{
	return ads1115_read_async(dev, sequence, NULL);
}

static void adc_context_start_sampling(struct adc_context * ctx)
{
	struct ads1115_data * data = CONTAINER_OF(ctx, struct ads1115_data, ctx);

	data->seq_channels = ctx->sequence.channels;
	data->repeat_buffer = data->buffer;

	k_sem_give(&data->sem);
}

static void adc_context_update_buffer_pointer(struct adc_context * ctx,
					      bool repeat_sampling)
{
	struct ads1115_data * data = CONTAINER_OF(ctx, struct ads1115_data, ctx);

	if (repeat_sampling) {
		data->buffer = data->repeat_buffer;
	}
}

/* Called within acquisition thread to read an individual sample */
/* Can return -EAGAIN if sample is not ready */
static int ads1115_read_channel(struct ads1115_data * data, uint8_t channel,
				uint16_t *result)
{
	int status = false;
	int err = 0;

	if(channel >= ADS1115_NUM_CHANNELS) {
		LOG_ERR("Invalid sampling channel");
		return -EINVAL;
	}

	if(!data->active) {
		//LOG_DBG("Starting fetching sample");
		err = ads1115_chan_change_with_start_conversion(data, channel);
		if(err) {
			LOG_ERR("Error starting conversion");
			return err;
		}

		data->active = true;
		data->active_channel = channel;
	}

	if(channel != data->active_channel) {
		LOG_ERR("Conversion active on channel %d, but requested on %d",
			data->active_channel, channel);
		return -EBUSY;
	}

	err = ads1115_once_conversion_status_get(data, &status);
	if(err) {
		LOG_ERR("Error checking status");
		return err;
	}

	if(!status) {
		return -EAGAIN;
	}

	err = ads1115_reg_read(data, ADS1115_REG_CONVERSION, result);
	if(err) {
		LOG_ERR("Error fetching sample");
		return err;
	}

	data->active = false;
	//LOG_DBG("Completed fetching sample %d", *result);

	return 0;
}

static void ads1115_acquisition_thread(struct ads1115_data *data)
{
	uint16_t result = 0;
	uint8_t channel;
	int err;
	uint16_t needed;
	uint16_t valid;
	uint16_t samples;
	uint64_t acc;

	while (true) {
		k_sem_take(&data->sem, K_FOREVER);

		acc = 0;
		samples = 0;
		valid = 0;
		needed = 1 << data->oversampling;

		while (data->seq_channels) {
			channel = find_lsb_set(data->seq_channels) - 1;

			//LOG_DBG("Reading channel %d", channel);

			err = ads1115_read_channel(data, channel, &result);
			if (err == -EAGAIN) {
				//LOG_DBG("Channel %d not yet ready", channel);
				/* Lie to adc_context_request_next_sampling to get back here */
				atomic_set(&data->ctx.sampling_requested, 0);
				k_sem_take(&data->sem, K_FOREVER);
			}
			else if (err) {
				LOG_ERR("failed to read channel %d (err %d)",
					channel, err);
				adc_context_complete(&data->ctx, err);
				break;
			} else {
				samples++;
			       	/* TODO: create good threshold attribute */
				//if(result > 10 && result < 0xfff0) {
				if(result < 0xfff0) {
					valid++;
					acc += result*result;
				}
				if (samples >= needed) {
					LOG_DBG("read channel %d, result = %d, acc = %ld", channel,
						result, (long)acc);
					if (needed > 1) {
						acc /= valid;
						*data->buffer++ = sqrt(acc);
					} else {
						*data->buffer++ = result;
					}
					WRITE_BIT(data->seq_channels, channel, 0);

					acc = 0;
					samples = 0;
					valid = 0;
				}
			}
		}

		adc_context_on_sampling_done(&data->ctx, data->dev);
	}
}

/*
static void ads1115_gpio_callback(const struct device * dev,
				  struct gpio_callback * cb, uint32_t pins)
{
	struct ads1115_data * data = dev->data;
	const struct ads1115_config * config = dev->config;

	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_DISABLE);
	k_sem_give(&data->sem);
}
*/

static int ads1115_reg_write(struct ads1115_data * data, uint8_t reg, uint16_t * val)
{
	int err = 0;

	uint8_t wr_buff[3] = {0};
	uint16_t wr_value = *val;

	wr_buff[0] = reg;
	wr_buff[1] = (wr_value >> 8) & 0xff;
	wr_buff[2] = wr_value & 0xff;

	err = i2c_write_dt(&data->cfg->bus, wr_buff, 3);

	if (err < 0) {
		LOG_ERR("Error writing %d to reg 0x%02x", err, reg);
		return err;
	}

	return 0;
}

static int ads1115_reg_read(struct ads1115_data * data, uint8_t reg, uint16_t * val)
{
	int err = 0;

	uint8_t wr_buff[1] = {0};
	uint8_t rd_buff[2] = {0};

	wr_buff[0] = reg;

	err = i2c_write_read_dt(&data->cfg->bus, wr_buff, 1, rd_buff, 2);
	if (err < 0) {
		LOG_ERR("Error writing %d to reg 0x%02x", err, reg);
		return err;
	}

	*val = (uint16_t)rd_buff[0] << 8;
	*val |= rd_buff[1];

	return 0;
}

static int ads1115_chan_change_with_start_conversion(struct ads1115_data * data, uint8_t channel)
{
	int err = 0;

	if(channel >= ADS1115_NUM_CHANNELS)
	{
		LOG_ERR("Invalid channel %d", channel);
		return -EINVAL;
	}

	ADS1115_CFG_MUX_SET(4 + channel);
	ADS1115_CFG_OS_SET(1);

	//LOG_DBG("Writing to config reg 0x%02x", data->config_reg);
	err = ads1115_reg_write(data, ADS1115_REG_CONFIG, &data->config_reg);
	if(err < 0)
	{
		return -EINVAL;
	}

	return 0;
}

static int ads1115_once_conversion_status_get(struct ads1115_data * data, int * status)
{
	int err = 0;

	err = ads1115_reg_read(data, ADS1115_REG_CONFIG, &data->config_reg);
	if(err < 0)
	{
		return -EINVAL;
	}

	*status = ADS1115_CFG_OS_GET();

	return 0;
}

static int ads1115_init(const struct device *dev)
{
	const struct ads1115_config * config = dev->config;
	struct ads1115_data * data = dev->data;
	int err = 0;

	data->cfg = config;

	k_sem_init(&data->sem, 0, 1);

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("I2C bus is not ready");
		return -ENODEV;
	}

	/* Try 100ms delay at the start */
	k_msleep(100);

	err = ads1115_reg_read(data, ADS1115_REG_CONFIG, &data->config_reg);
	if(err < 0)
	{
		LOG_ERR("Unable to read config register");
		return -EINVAL;
	}

	ADS1115_CFG_OS_SET(0);		/* OS = No conversion start */
	ADS1115_CFG_MUX_SET(4);		/* MUX = 100b: AINp = AIN0 and AINn = GND */
	ADS1115_CFG_PGA_SET(1);		/* PGA = 001b: FSR = 4.096V */
	if(config->continuous_mode)
		ADS1115_CFG_MODE_SET(0);/* MODE = 0: Continuous-conversion mode */
	else
		ADS1115_CFG_MODE_SET(1);/* MODE = 1: Single-shot mode or power-down state */
	//ADS1115_CFG_DR_SET(5);		/* DR = 101b: 250 samples per second */
	ADS1115_CFG_DR_SET(6);		/* DR = 110b: 475 samples per second */
	ADS1115_CFG_CM_SET(0);		/* COMP_MODE = 0: Traditional comparator */
	ADS1115_CFG_CP_SET(1);		/* COMP_POL = 1: Active high polarity */
	ADS1115_CFG_CL_SET(0);		/* COMP_LAT = 0: Nonlatching comparator */
	ADS1115_CFG_CQ_SET(3);		/* COMP_QUE = 11b: Disable comparator */
	err = ads1115_reg_write(data, ADS1115_REG_CONFIG, &data->config_reg);
	if(err < 0)
	{
		LOG_ERR("Unable to write config register");
		return -EINVAL;
	}

	k_thread_create(&data->thread, data->stack,
			CONFIG_ADC_ADS1115_ACQUISITION_THREAD_STACK_SIZE,
			(k_thread_entry_t)ads1115_acquisition_thread,
			data, NULL, NULL,
			CONFIG_ADC_ADS1115_ACQUISITION_THREAD_PRIO,
			0, K_NO_WAIT);

	adc_context_unlock_unconditionally(&data->ctx);

	return 0;
}

#define ADS1115_DEV(inst) DT_INST(inst, DT_DRV_COMPAT)
#define ADS1115_PROP(inst, prop) DT_PROP(DT_INST(inst, DT_DRV_COMPAT), prop)

#define DEFINE_ADS1115(inst)						\
									\
static const struct ads1115_config ads1115_config_##inst = {		\
	.bus = I2C_DT_SPEC_INST_GET(inst),				\
	.continuous_mode = ADS1115_PROP(inst,continuous_mode),		\
	.int_gpio = GPIO_DT_SPEC_GET_OR(ADS1115_DEV(inst), int_gpios, {.port=NULL}),	\
};									\
									\
static struct ads1115_data ads1115_data_##inst = {			\
	ADC_CONTEXT_INIT_TIMER(ads1115_data_##inst, ctx), 		\
	ADC_CONTEXT_INIT_LOCK(ads1115_data_##inst, ctx),		\
	ADC_CONTEXT_INIT_SYNC(ads1115_data_##inst, ctx),		\
	.active = false,						\
	.cfg = &ads1115_config_##inst,					\
};									\
									\
DEVICE_DT_INST_DEFINE(inst,						\
		ads1115_init,						\
		device_pm_control_nop,					\
		&ads1115_data_##inst,					\
		&ads1115_config_##inst,					\
		POST_KERNEL,						\
		CONFIG_SENSOR_INIT_PRIORITY,				\
		&ads1115_api_funcs);

/*
	.avg_method = ADS1115_PROP(inst,avg_method),			\
*/

DT_INST_FOREACH_STATUS_OKAY(DEFINE_ADS1115)

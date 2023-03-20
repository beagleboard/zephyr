/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 * Copyright (c) 2020 Jason Kridner
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/util.h>
#include <zephyr/random/rand32.h>
#include <zephyr/devicetree.h>
#include <zephyr/devicetree/io-channels.h>
#include <errno.h>
#include <zephyr/linker/sections.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_mgmt.h>

#include <math.h>

#define LOG_LEVEL LOG_LEVEL_INF
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensortest);

#define MCAST_IP6ADDR "ff84::2"

#define BLINK_MS 500

#define MAX_STR_LEN 256
static char outstr[MAX_STR_LEN];

enum api {
	BUTTON_API,
	SENSOR_API,
};

enum edev {
	BUTTON,
	LIGHT,
	ACCEL,
	HUMIDITY,
	NUM_DEVICES,
};

struct led_work {
	uint8_t active_led;
	struct k_work_delayable dwork;
};

static void sensor_work_handler(struct k_work *work);

static const char *device_labels[NUM_DEVICES] = {
	[BUTTON] = DT_PROP(DT_ALIAS(sw0), label),
	[LIGHT] = "LIGHT",
	[ACCEL] = "ACCEL",
	[HUMIDITY] = "HUMIDITY",
};

static const char *device_names[NUM_DEVICES] = {
	[BUTTON] = DEVICE_DT_NAME(DT_GPIO_CTLR(DT_ALIAS(sw0), gpios)),
	[LIGHT] = "OPT3001-LIGHT",
	[ACCEL] = "LIS2DE12-ACCEL",
	[HUMIDITY] = "HDC2010-HUMIDITY",
};

static const uint8_t device_pins[NUM_DEVICES] = {
	[BUTTON] = DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
};

static const enum api apis[NUM_DEVICES] = {
	BUTTON_API,
	SENSOR_API, /* LIGHT */
	SENSOR_API, /* ACCEL */
	SENSOR_API, /* HUMIDITY */
};

static struct device *devices[NUM_DEVICES];

static struct led_work led_work;
K_WORK_DEFINE(sensor_work, sensor_work_handler);
static struct gpio_callback button_callback;

static struct sockaddr_in6 addr;
static int fd = -1;

/* Set TIMED_SENSOR_READ to 0 to disable */
#define TIMED_SENSOR_READ 6
static int sensor_read_count = TIMED_SENSOR_READ;

static void setup_telnet_ipv6(struct net_if *iface)
{
	char hr_addr[NET_IPV6_ADDR_LEN];
	struct in6_addr addr;

	if (net_addr_pton(AF_INET6, CONFIG_NET_CONFIG_MY_IPV6_ADDR, &addr)) {
		LOG_ERR("Invalid address: %s", CONFIG_NET_CONFIG_MY_IPV6_ADDR);
		return;
	}

	net_if_ipv6_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);

	LOG_INF("IPv6 address: %s",
		net_addr_ntop(AF_INET6, &addr, hr_addr,
					 NET_IPV6_ADDR_LEN));

	if (net_addr_pton(AF_INET6, MCAST_IP6ADDR, &addr)) {
		LOG_ERR("Invalid address: %s", MCAST_IP6ADDR);
		return;
	}

	net_if_ipv6_maddr_add(iface, &addr);
}

static void led_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	int r;

	/*
	uint8_t prev_led;

	LOG_DBG("%s(): active_led: %u", __func__, led_work.active_led);

	if (led_work.active_led == LED_SUBG) {
		prev_led = LED_24G;
	} else {
		prev_led = LED_SUBG;
	}

	r = gpio_pin_set(devices[prev_led], device_pins[prev_led], 0);
	__ASSERT(r == 0, "failed to turn off led %u: %d", prev_led, r);

	r = gpio_pin_set(devices[led_work.active_led],
			 device_pins[led_work.active_led], 1);
	__ASSERT(r == 0, "failed to turn on led %u: %d", led_work.active_led,
		 r);

	if (led_work.active_led == LED_SUBG) {
		led_work.active_led = LED_24G;
	} else {
		led_work.active_led = LED_SUBG;
	}
	*/

	r = k_work_schedule(&led_work.dwork, K_MSEC(BLINK_MS));
	__ASSERT(r == 0, "k_work_schedule() failed for LED %u work: %d",
		 led_work.active_led, r);

	if (sensor_read_count > 0) {
		sensor_read_count--;
		if (sensor_read_count <= 0) {
			sensor_read_count = TIMED_SENSOR_READ;
			if(TIMED_SENSOR_READ > 0)
				sensor_read_count +=
				       	sys_rand32_get() % TIMED_SENSOR_READ;
			k_work_submit(&sensor_work);
		}
	}
}

static void print_sensor_value(size_t idx, const char *chan,
			       struct sensor_value *val)
{
	LOG_INF("%s: %s%d,%d", device_labels[idx], chan, val->val1, val->val2);

	sprintf(outstr+strlen(outstr), "%d%c:", idx, chan[0]);
	sprintf(outstr+strlen(outstr), "%d", val->val1);
	if (val->val2 != 0) {
		sprintf(outstr+strlen(outstr), ".%02d;", abs(val->val2) / 10000);
	} else {
		sprintf(outstr+strlen(outstr), ";");
	}
}

static void send_sensor_value()
{
	if ((fd >= 0) && (strlen(outstr) > 0)) {
		sendto(fd, outstr, strlen(outstr), 0,
			(const struct sockaddr *) &addr,
			sizeof(addr));
	}

	outstr[0] = '\0';
}

static void sensor_work_handler(struct k_work *work)
{
	struct sensor_value val;

	outstr[0] = '\0';

	for (size_t i = 0; i < NUM_DEVICES; ++i) {
		if (apis[i] != SENSOR_API) {
			continue;
		}

		if (devices[i] == NULL) {
			continue;
		}

		sensor_sample_fetch(devices[i]);

		if (i == LIGHT) {
			sensor_channel_get(devices[i], SENSOR_CHAN_LIGHT, &val);
			print_sensor_value(i, "l: ", &val);
			send_sensor_value();
			continue;
		}

		if (i == ACCEL) {
			sensor_channel_get(devices[i], SENSOR_CHAN_ACCEL_X,
					   &val);
			print_sensor_value(i, "x: ", &val);
			sensor_channel_get(devices[i], SENSOR_CHAN_ACCEL_Y,
					   &val);
			print_sensor_value(i, "y: ", &val);
			sensor_channel_get(devices[i], SENSOR_CHAN_ACCEL_Z,
					   &val);
			print_sensor_value(i, "z: ", &val);
			send_sensor_value();
			continue;
		}

		if (i == HUMIDITY) {
			sensor_channel_get(devices[i], SENSOR_CHAN_HUMIDITY,
					   &val);
			print_sensor_value(i, "h: ", &val);
			sensor_channel_get(devices[i], SENSOR_CHAN_AMBIENT_TEMP,
					   &val);
			print_sensor_value(i, "t: ", &val);
			send_sensor_value();
			continue;
		}
	}
}

static void button_handler(struct device *port, struct gpio_callback *cb,
			   gpio_port_pins_t pins)
{
	ARG_UNUSED(port);

	gpio_pin_t pin = device_pins[BUTTON];
	gpio_port_pins_t mask = BIT(pin);

	if ((mask & cb->pin_mask) != 0) {
		if ((mask & pins) != 0) {
			/* BEL (7) triggers BEEP on MSP430 */
			LOG_INF("%c%s event", 7, device_labels[BUTTON]);
			/* print sensor readings */
			k_work_submit(&sensor_work);
		}
	}
}

void main(void)
{
	int r;
	struct net_if *iface = net_if_get_default();

	outstr[0] = '\0';

	fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		LOG_ERR("failed to open socket");
	} else {
		memset(&addr, 0, sizeof(struct sockaddr_in6));
		addr.sin6_family = AF_INET6;
		addr.sin6_port = htons(9999);
		inet_pton(AF_INET6, "ff02::1", &addr.sin6_addr);
	}

	setup_telnet_ipv6(iface);

	for (size_t i = 0; i < NUM_DEVICES; ++i) {
		LOG_INF("opening device %s", device_labels[i]);
		devices[i] =
			(struct device *)device_get_binding(device_names[i]);
		if (devices[i] == NULL) {
			LOG_ERR("failed to open device %s", device_labels[i]);
			continue;
		}

		/* per-device setup */
		switch (apis[i]) {
		/*
		case LED_API:
			r = gpio_pin_configure(devices[i], device_pins[i],
					       GPIO_OUTPUT_LOW);
			__ASSERT(r == 0,
				 "gpio_pin_configure(%s, %u, %x) failed: %d",
				 device_labels[i], device_pins[i],
				 GPIO_OUTPUT_LOW, r);
			break;
		*/
		case BUTTON_API:
			r = gpio_pin_configure(
				devices[i], device_pins[i],
				(GPIO_INPUT |
				 DT_GPIO_FLAGS(DT_ALIAS(sw0), gpios)));
			__ASSERT(r == 0,
				 "gpio_pin_configure(%s, %u, %x) failed: %d",
				 device_labels[i], device_pins[i],
				 (GPIO_INPUT |
				  DT_GPIO_FLAGS(DT_ALIAS(sw0), gpios)),
				 r);
			r = gpio_pin_interrupt_configure(
				devices[i], device_pins[i],
				GPIO_INT_EDGE_TO_ACTIVE);
			__ASSERT(
				r == 0,
				"gpio_pin_interrupt_configure(%s, %u, %x) failed: %d",
				device_labels[i], device_pins[i],
				GPIO_INT_EDGE_TO_ACTIVE, r);
			break;

		case SENSOR_API:
			break;

		default:
			break;
		}
	}

	/* setup timer-driven LED event */
	k_work_init_delayable(&led_work.dwork, led_work_handler);
	//led_work.active_led = LED_SUBG;
	r = k_work_schedule(&led_work.dwork, K_MSEC(BLINK_MS));
	__ASSERT(r == 0, "k_work_schedule() failed for LED %u work: %d",
		 LED_SUBG, r);

	/* setup input-driven button event */
	gpio_init_callback(
		&button_callback, (gpio_callback_handler_t)button_handler,
		BIT(device_pins[BUTTON]));
	r = gpio_add_callback(devices[BUTTON], &button_callback);
	__ASSERT(r == 0, "gpio_add_callback() failed: %d", r);

	for (;;) {
		k_sleep(K_MSEC(1000));
	}
}

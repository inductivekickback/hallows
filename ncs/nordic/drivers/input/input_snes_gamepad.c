/*
 * Copyright (c) 2026 Daniel Veilleux
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT dmv_snes_gamepad

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#define SNES_GAMEPAD_WAIT_US	5

LOG_MODULE_REGISTER(input_snes_gamepad, CONFIG_INPUT_LOG_LEVEL);

static const uint16_t SNES_GAMEPAD_CODES[12] = {
    INPUT_BTN_SOUTH,/* B */
    INPUT_BTN_WEST, /* Y */
    INPUT_BTN_SELECT,
    INPUT_BTN_START,
    INPUT_BTN_DPAD_UP,
    INPUT_BTN_DPAD_DOWN,
    INPUT_BTN_DPAD_LEFT,
    INPUT_BTN_DPAD_RIGHT,
    INPUT_BTN_EAST,	/* A */
    INPUT_BTN_NORTH,/* X */
    INPUT_BTN_TL,	/* L */
    INPUT_BTN_TR,	/* R */
};

struct snes_gamepad_config {
	uint32_t polling_interval_ms;
	struct gpio_dt_spec latch_spec;
	struct gpio_dt_spec data_spec;
	struct gpio_dt_spec clock_spec;
};

struct snes_gamepad_data {
	const struct device *dev;
	struct k_work_delayable work;
	k_timepoint_t next;
	uint16_t pin_state;

#ifdef CONFIG_PM_DEVICE
	atomic_t suspended;
#endif
};

static void snes_gamepad_poll(struct k_work *work)
{
	int ret;
	uint16_t changed;
	uint16_t state = 0;
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct snes_gamepad_data *data = CONTAINER_OF(dwork,
    				struct snes_gamepad_data, work);
    const struct device *dev = data->dev;
    const struct snes_gamepad_config *cfg = dev->config;

#ifdef CONFIG_PM_DEVICE
    if (unlikely(atomic_get(&data->suspended))) {
    	return;
    }
#endif

    data->next.tick += k_ms_to_ticks_ceil64(cfg->polling_interval_ms);
    if (sys_timepoint_expired(data->next)) {
        data->next = sys_timepoint_calc(K_MSEC(cfg->polling_interval_ms));
    }
    ret = k_work_reschedule(dwork, sys_timepoint_timeout(data->next));
    if (unlikely(ret != 1)) {
    	k_panic();
    }

    ret = gpio_pin_set_dt(&cfg->latch_spec, 1);
    if (unlikely(ret != 0)) {
    	return;
    }
    k_busy_wait(SNES_GAMEPAD_WAIT_US);
    ret = gpio_pin_set_dt(&cfg->latch_spec, 0);
    if (unlikely(ret != 0)) {
    	return;
    }
    k_busy_wait(SNES_GAMEPAD_WAIT_US);

    for (uint16_t mask = 0x0001; mask < 0x1000; mask <<= 1) {
    	ret = gpio_pin_get_dt(&cfg->data_spec);
    	if (unlikely(ret < 0)) {
    		return;
    	}
		if (ret) {
		    state |= mask;
		}
		ret = gpio_pin_set_dt(&cfg->clock_spec, 1);
    	if (unlikely(ret != 0)) {
    		return;
    	}
    	k_busy_wait(SNES_GAMEPAD_WAIT_US);
    	ret = gpio_pin_set_dt(&cfg->clock_spec, 0);
    	if (unlikely(ret != 0)) {
    		return;
    	}
	   	k_busy_wait(SNES_GAMEPAD_WAIT_US);
    }

    changed = state ^ data->pin_state;
    if (!changed) {
        return;
    }

    int last = (find_msb_set(changed) - 1);
    for (int i = 0; i < 12; i++) {
        if (changed & BIT(i)) {
            input_report_key(dev, SNES_GAMEPAD_CODES[i], !!(state & BIT(i)),
					i == last, K_FOREVER);
        }
    }
    data->pin_state = state;
}

static int snes_gamepad_init(const struct device *dev)
{
	int ret;
    struct snes_gamepad_data *data = dev->data;
    const struct snes_gamepad_config *cfg = dev->config;

    data->dev = dev;
    k_work_init_delayable(&data->work, snes_gamepad_poll);

    ret = gpio_pin_configure_dt(&cfg->latch_spec, GPIO_OUTPUT_LOW);
    if (ret) {
    	return -1;
    }
    ret = gpio_pin_configure_dt(&cfg->clock_spec, GPIO_OUTPUT_LOW);
	if (ret) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&cfg->data_spec, GPIO_INPUT | GPIO_ACTIVE_LOW);
	if (ret) {
		return -1;
	}

    data->next = sys_timepoint_calc(K_MSEC(cfg->polling_interval_ms));
    ret = k_work_schedule(&data->work, sys_timepoint_timeout(data->next));
    if (unlikely(ret != 1)) {
    	return -1;
    }
    return 0;
}

#ifdef CONFIG_PM_DEVICE
static int snes_gamepad_pm_action(const struct device *dev,
					enum pm_device_action action)
{
	int ret;
	const struct snes_gamepad_config *cfg = dev->config;
	struct snes_gamepad_data *data = dev->data;

	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
		atomic_set(&data->suspended, 1);
		(void)k_work_cancel_delayable(&data->work);
		return 0;
	case PM_DEVICE_ACTION_RESUME:
		atomic_set(&data->suspended, 0);
	    data->next = sys_timepoint_calc(K_MSEC(cfg->polling_interval_ms));
	    ret = k_work_schedule(&data->work, sys_timepoint_timeout(data->next));
	    if (unlikely(ret != 1)) {
	    	return -1;
	    }
		return 0;
	default:
		return -ENOTSUP;
	}
}
#endif

#define SNES_GAMEPAD_CHECK_FLAGS(i)                                      	\
	BUILD_ASSERT(DT_INST_GPIO_FLAGS(i, latch_gpios) == 0 &&              	\
				 DT_INST_GPIO_FLAGS(i, clock_gpios) == 0 &&              	\
				 DT_INST_GPIO_FLAGS(i, data_gpios) == 0,                 	\
				 "snes-gamepad driver overrides GPIO flags; set them to 0.");

#define SNES_GAMEPAD_CHECK_INTERVAL(i)										\
	BUILD_ASSERT(IN_RANGE(DT_INST_PROP(i, polling_interval_ms), 1, 1000),	\
	"polling-interval-ms must be between 1 and 1000");

#define SNES_GAMEPAD_INIT(i)												\
	SNES_GAMEPAD_CHECK_FLAGS(i)												\
	SNES_GAMEPAD_CHECK_INTERVAL(i)											\
	static const struct snes_gamepad_config snes_gamepad_cfg_##i = {        \
		.polling_interval_ms = DT_INST_PROP(i, polling_interval_ms),		\
		.latch_spec = GPIO_DT_SPEC_INST_GET(i, latch_gpios),     			\
		.data_spec = GPIO_DT_SPEC_INST_GET(i, data_gpios),     				\
		.clock_spec = GPIO_DT_SPEC_INST_GET(i, clock_gpios),     			\
	};																		\
																			\
	static struct snes_gamepad_data snes_gamepad_data_##i;					\
																			\
	PM_DEVICE_DT_INST_DEFINE(i, snes_gamepad_pm_action);					\
																			\
	DEVICE_DT_INST_DEFINE(i, &snes_gamepad_init, PM_DEVICE_DT_INST_GET(i),  \
			      &snes_gamepad_data_##i, &snes_gamepad_cfg_##i,            \
			      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(SNES_GAMEPAD_INIT)

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <soc.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <bluetooth/services/hallows.h>

#include <zephyr/settings/settings.h>

#include <dk_buttons_and_leds.h>

#include "hallows.h"

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define RED_LED                 1
#define GREEN_LED               2
#define BLUE_LED                3

#define CONN_STATUS_LED         3

#define DISPENSE_WAIT_MS		1000
#define STATUS_WAIT_S			30

#define CMD_Q_LEN               10

#define LOG_MODULE_NAME hallows
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

static struct bt_hallows hallows;
static struct k_work adv_work;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_HALLOWS_VAL),
};

K_MSGQ_DEFINE(hallows_msgq, sizeof(uint8_t), CMD_Q_LEN, sizeof(uint8_t));

static void dispense(char index)
{
    LOG_INF("dispense(%d)", index);

    uint8_t msg = (CMD_BIT|OPCODE_DISPENSE|index);
    k_msgq_put(&hallows_msgq, &msg, K_NO_WAIT);
}

void dispense_thread(void)
{
    uint8_t msg;

    for(;;) {
    	k_msgq_get(&hallows_msgq, &msg, K_FOREVER);
    	dk_set_led_on(GREEN_LED);
    	uart_poll_out(uart, msg);
    	k_sleep(K_MSEC(DISPENSE_WAIT_MS));
    	dk_set_led_off(GREEN_LED);
	}
}

static struct bt_hallows_cb hallows_cb = {
    .dispense_cb = dispense,
};

static void adv_work_handler(struct k_work *work)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Advertising successfully started\n");
}

static void advertising_start(void)
{
    k_work_submit(&adv_work);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
        return;
    }

    printk("Connected\n");

    dk_set_led_on(CONN_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));

    dk_set_led_off(CONN_STATUS_LED);
}

static void recycled_cb(void)
{
    printk("Connection object available from previous conn. Disconnect is complete!\n");
    advertising_start();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected        = connected,
    .disconnected     = disconnected,
    .recycled         = recycled_cb,
};

int main(void)
{
    int err;

    printk("Starting Hallows machine\n");

    if (!device_is_ready(uart)) {
        printk("UART device not ready\r\n");
        return -1;
    }

    err = dk_leds_init();
    if (err) {
        printk("LEDs init failed (err %d)\n", err);
        return 0;
    }

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    err = bt_hallows_init(&hallows, &hallows_cb);
    if (err) {
        printk("Failed to init service (err:%d)\n", err);
        return 0;
    }

    k_work_init(&adv_work, adv_work_handler);
    advertising_start();

    uint8_t msg;
    for (;;) {
    	k_sleep(K_SECONDS(STATUS_WAIT_S));
        msg = (CMD_BIT|OPCODE_STATUS);
	    k_msgq_put(&hallows_msgq, &msg, K_NO_WAIT);
    }
}

K_THREAD_DEFINE(dispsense_thread_id, 1024, dispense_thread, NULL, NULL, NULL, -1, 0, 0);

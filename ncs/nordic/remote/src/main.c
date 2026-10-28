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
#include <zephyr/drivers/gpio/gpio_sx1509b.h>
#include <soc.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <zephyr/pm/device.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <bluetooth/services/hallows.h>
#include <bluetooth/services/hallows_client.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>

#include <zephyr/settings/settings.h>

#include <accel.h>

#define LOG_MODULE_NAME hallows
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define DK_BTN_COUNT    4

#define CMD_WAIT_MS     1000
#define CMD_Q_LEN       10

#define SLEEP_TIMER_S   10

static const struct led_dt_spec led_r_lw = LED_DT_SPEC_GET(DT_NODELABEL(led0));
static const struct led_dt_spec led_g_lw = LED_DT_SPEC_GET(DT_NODELABEL(led1));
static const struct led_dt_spec led_b_lw = LED_DT_SPEC_GET(DT_NODELABEL(led2));

static const
struct device *const gamepad = DEVICE_DT_GET(DT_NODELABEL(snes_gamepad));

#define NUMBER_OF_LEDS 3
#define RED_LED DT_GPIO_PIN(DT_NODELABEL(led0), gpios)
#define GREEN_LED DT_GPIO_PIN(DT_NODELABEL(led1), gpios)
#define BLUE_LED DT_GPIO_PIN(DT_NODELABEL(led2), gpios)

static const struct device *sx1509b_dev = DEVICE_DT_GET(DT_NODELABEL(sx1509b));

static void err_flash(void);
static void sleep_expiry_function(struct k_timer *timer_id);
static void enter_sleep(struct k_work *work);

K_TIMER_DEFINE(sleep_timer, sleep_expiry_function, NULL);
K_MSGQ_DEFINE(cmd_msgq, sizeof(uint8_t), CMD_Q_LEN, sizeof(uint8_t));

K_WORK_DEFINE(sleep_work, enter_sleep);

static struct k_work scan_work;
static bool awake;
static atomic_t reconnect;

static struct bt_conn *default_conn;
static struct bt_hallows_client hallows_client;

static int scan_start(void);

static void enter_sleep(struct k_work *work)
{
    int ret;
    atomic_set(&reconnect, 0);
    awake = false;

    ret = pm_device_action_run(gamepad, PM_DEVICE_ACTION_SUSPEND);
    if (ret) {
        err_flash();
    }

    if (default_conn) {
        ret = bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        if (ret) {
            err_flash();
    }
}

    led_off_dt(&led_r_lw);
}

static void sleep_expiry_function(struct k_timer *timer_id)
{
    k_work_submit(&sleep_work);
}

static void motion_cb(const struct device *dev, const struct sensor_trigger *trig)
{
    int ret;

    if (awake) {
        k_timer_start(&sleep_timer, K_SECONDS(SLEEP_TIMER_S), K_FOREVER);
        return;
    }

    atomic_set(&reconnect, 1);
    ret = scan_start();
    if (ret) {
        err_flash();
    }

    ret = pm_device_action_run(gamepad, PM_DEVICE_ACTION_RESUME);
    if (ret) {
        err_flash();
    }

    awake = true;
    led_on_dt(&led_r_lw);
    k_timer_start(&sleep_timer, K_SECONDS(SLEEP_TIMER_S), K_FOREVER);
}

static void ble_data_sent(uint8_t err)
{
    LOG_DBG("ble_data_sent(%d)", err);
}

static struct bt_hallows_client_cb hallows_cb = {
    .sent = ble_data_sent,
};

static void discovery_complete(struct bt_gatt_dm *dm,
                   void *context)
{
    struct bt_hallows_client *hallows = context;
    LOG_INF("Service discovery completed");

    bt_gatt_dm_data_print(dm);
    bt_hallows_handles_assign(dm, hallows);
    bt_gatt_dm_data_release(dm);
}

static void discovery_service_not_found(struct bt_conn *conn,
                    void *context)
{
    LOG_INF("Service not found");
}

static void discovery_error(struct bt_conn *conn,
                int err,
                void *context)
{
    LOG_WRN("Error while discovering GATT database: (%d)", err);
}

struct bt_gatt_dm_cb discovery_cb = {
    .completed         = discovery_complete,
    .service_not_found = discovery_service_not_found,
    .error_found       = discovery_error,
};

static void gatt_discover(struct bt_conn *conn)
{
    int err;

    if (conn != default_conn) {
        return;
    }

    err = bt_gatt_dm_start(conn,
                   BT_UUID_HALLOWS,
                   &discovery_cb,
                   &hallows_client);
    if (err) {
        LOG_ERR("could not start the discovery procedure, error "
            "code: %d", err);
    }
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    int err;

    led_on_dt(&led_b_lw);

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (conn_err) {
        LOG_INF("Failed to connect to %s, 0x%02x %s", addr, conn_err,
            bt_hci_err_to_str(conn_err));

        if (default_conn == conn) {
            bt_conn_unref(default_conn);
            default_conn = NULL;

            (void)k_work_submit(&scan_work);
        }

        return;
    }

    LOG_INF("Connected: %s", addr);

    gatt_discover(conn);

    err = bt_scan_stop();
    if (err && (err != -EALREADY)) {
        LOG_ERR("Stop LE scan failed (err %d)", err);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    led_off_dt(&led_b_lw);

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected: %s, reason 0x%02x %s", addr, reason, bt_hci_err_to_str(reason));

    if (default_conn != conn) {
        return;
    }

    bt_conn_unref(default_conn);
    default_conn = NULL;

    if (atomic_get(&reconnect)) {
        (void)k_work_submit(&scan_work);
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected        = connected,
    .disconnected     = disconnected,
};

static void scan_filter_match(struct bt_scan_device_info *device_info,
                  struct bt_scan_filter_match *filter_match,
                  bool connectable)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

    LOG_INF("Filters matched. Address: %s connectable: %d",
        addr, connectable);
}

static void scan_connecting_error(struct bt_scan_device_info *device_info)
{
    LOG_WRN("Connecting failed");
}

static void scan_connecting(struct bt_scan_device_info *device_info,
                struct bt_conn *conn)
{
    default_conn = bt_conn_ref(conn);
}

static int hallows_client_init(void)
{
    int err = bt_hallows_client_init(&hallows_client, &hallows_cb);
    if (err) {
        LOG_ERR("Hallows client initialization failed (err %d)", err);
        return err;
    }

    LOG_INF("Hallows client module initialized");
    return err;
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL,
        scan_connecting_error, scan_connecting);

static void try_add_address_filter(const struct bt_bond_info *info, void *user_data)
{
    int err;
    char addr[BT_ADDR_LE_STR_LEN];
    uint8_t *filter_mode = user_data;

    bt_addr_le_to_str(&info->addr, addr, sizeof(addr));

    struct bt_conn *conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &info->addr);

    if (conn) {
        bt_conn_unref(conn);
        return;
    }

    err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_ADDR, &info->addr);
    if (err) {
        LOG_ERR("Address filter cannot be added (err %d): %s", err, addr);
        return;
    }

    LOG_INF("Address filter added: %s", addr);
    *filter_mode |= BT_SCAN_ADDR_FILTER;
}

static int scan_start(void)
{
    int err;
    uint8_t filter_mode = 0;

    err = bt_scan_stop();
    if (err) {
        LOG_ERR("Failed to stop scanning (err %d)", err);
        return err;
    }

    if (!atomic_get(&reconnect)) {
        return 0;
    }

    bt_scan_filter_remove_all();

    err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_HALLOWS);
    if (err) {
        LOG_ERR("UUID filter cannot be added (err %d", err);
        return err;
    }
    filter_mode |= BT_SCAN_UUID_FILTER;

    bt_foreach_bond(BT_ID_DEFAULT, try_add_address_filter, &filter_mode);

    err = bt_scan_filter_enable(filter_mode, false);
    if (err) {
        LOG_ERR("Filters cannot be turned on (err %d)", err);
        return err;
    }

    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        LOG_ERR("Scanning failed to start (err %d)", err);
        return err;
    }

    LOG_INF("Scan started");
    return 0;
}

static void scan_work_handler(struct k_work *item)
{
    ARG_UNUSED(item);

    (void)scan_start();
}

static void scan_init(void)
{
    struct bt_scan_init_param scan_init = {
        .connect_if_match = true,
    };

    bt_scan_init(&scan_init);
    bt_scan_cb_register(&scan_cb);

    k_work_init(&scan_work, scan_work_handler);
    LOG_INF("Scan module initialized");
}

void cmd_thread(void)
{
    uint8_t msg;
    for(;;) {
        k_msgq_get(&cmd_msgq, &msg, K_FOREVER);
        led_off_dt(&led_r_lw);
        led_off_dt(&led_b_lw);
        led_on_dt(&led_g_lw);
        if (default_conn == NULL) {
            printk("Not connected.\n");
        } else {
            int err = bt_hallows_send(&hallows_client, &msg, sizeof(msg));
            if (err) {
                printk("bt_hallows_send failed: %d\n", err);
            }
        }
        k_sleep(K_MSEC(CMD_WAIT_MS));
        led_off_dt(&led_g_lw);
        led_on_dt(&led_r_lw);
        led_on_dt(&led_b_lw);
    }
}

static void gamepad_cb(struct input_event *evt, void *user_data)
{
    if (evt->type == INPUT_EV_KEY) {
        LOG_INF("key code=0x%x %s", evt->code, evt->value ? "down" : "up");
        if (evt->value) {
            uint8_t data;
            switch (evt->code) {
            case INPUT_BTN_WEST: /* Y */
                data = '0';
                k_msgq_put(&cmd_msgq, &data, K_NO_WAIT);
                break;
            case INPUT_BTN_NORTH: /* X */
                data = '1';
                k_msgq_put(&cmd_msgq, &data, K_NO_WAIT);
                break;
            case INPUT_BTN_SOUTH: /* B */
                data = '2';
                k_msgq_put(&cmd_msgq, &data, K_NO_WAIT);
                break;
            case INPUT_BTN_EAST: /* A */
                data = '3';
                k_msgq_put(&cmd_msgq, &data, K_NO_WAIT);
                break;
            }
        }
    }

    if (evt->sync) {
        k_timer_start(&sleep_timer, K_SECONDS(SLEEP_TIMER_S), K_FOREVER);
    }
}

INPUT_CALLBACK_DEFINE(gamepad, gamepad_cb, NULL);

static void err_flash(void)
{
    while (1) {
        led_on_dt(&led_r_lw);
        k_sleep(K_MSEC(200));
        led_off_dt(&led_r_lw);
        k_sleep(K_MSEC(100));
    }
}

int main(void)
{
    int err;

    if (!device_is_ready(sx1509b_dev)) {
        return -1;
    }

    if (!device_is_ready(gamepad)) {
        goto ERROR_EXIT;
    }

    err = motion_init(motion_cb);
    if (err) {
        goto ERROR_EXIT;
    }

    awake = true;
    led_on_dt(&led_r_lw);
    k_timer_start(&sleep_timer, K_SECONDS(SLEEP_TIMER_S), K_FOREVER);

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        goto ERROR_EXIT;
    }

    printk("Bluetooth initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    err = hallows_client_init();
    if (err) {
        printk("Failed to init client (err:%d)\n", err);
        goto ERROR_EXIT;
    }

    scan_init();
    atomic_set(&reconnect, 1);
    err = scan_start();
    if (err) {
        goto ERROR_EXIT;
    }
    return 0;

ERROR_EXIT:
    err_flash();
}

K_THREAD_DEFINE(cmd_thread_id, 1024, cmd_thread, NULL, NULL, NULL, -1, 0, 0);

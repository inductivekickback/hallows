#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <bluetooth/services/hallows.h>

LOG_MODULE_REGISTER(bt_hallows, CONFIG_BT_HALLOWS_LOG_LEVEL);

enum {
	HALLOWS_INITIALIZED
};

static const struct bt_hallows_cb *callback;

static ssize_t dispense_received(struct bt_conn *conn,
					const struct bt_gatt_attr *attr,
					const void *buf, uint16_t len,
					uint16_t offset, uint8_t flags)
{
	uint8_t *data = (uint8_t*)buf;

	LOG_DBG("Received dispense request, data %p length %u", buf, len);

	if (len != 1) {
		LOG_WRN("Invalid dispenser index len: %d", len);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if (offset != 0) {
		LOG_WRN("Invalid data offset");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (data[0] < '0' || data[0] > '3') {
		LOG_WRN("Invalid dispenser index: %c", data[0]);
		return BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE);
	}

	if (callback && callback->dispense_cb) {
		callback->dispense_cb(data[0] - '0');
	}

	return len;
}

BT_GATT_SERVICE_DEFINE(hallows_svc,
BT_GATT_PRIMARY_SERVICE(BT_UUID_HALLOWS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HALLOWS_CHAR,
		BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_WRITE,
		NULL, dispense_received, NULL),
);

int bt_hallows_init(struct bt_hallows *hallows, const struct bt_hallows_cb *cb)
{
	if (!hallows) {
		return -EINVAL;
	}

	if (atomic_test_and_set_bit(&hallows->state, HALLOWS_INITIALIZED)) {
		return -EALREADY;
	}

	callback = cb;
	return 0;
}

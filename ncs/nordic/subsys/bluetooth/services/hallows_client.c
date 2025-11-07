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
#include <bluetooth/services/hallows_client.h>

LOG_MODULE_REGISTER(bt_hallows_client, CONFIG_BT_HALLOWS_CLIENT_LOG_LEVEL);

enum {
	HALLOWS_INITIALIZED,
	HALLOWS_ASYNC_WRITE_PENDING
};

static const struct bt_hallows_client_cb *callback;

static void received_hallows_response(struct bt_conn *conn, uint8_t err,
				      struct bt_gatt_write_params *params)
{
	struct bt_hallows_client *hallows;

	ARG_UNUSED(conn);
	ARG_UNUSED(params);

	hallows = CONTAINER_OF(params, struct bt_hallows_client,
			       hallows_params);

	atomic_clear_bit(&hallows->state, HALLOWS_ASYNC_WRITE_PENDING);

	if (callback && callback->sent) {
		callback->sent(err);
	}
}

int bt_hallows_client_init(struct bt_hallows_client *hallows,
			   const struct bt_hallows_client_cb *cb)
{
	if (!hallows) {
		return -EINVAL;
	}

	if (atomic_test_and_set_bit(&hallows->state, HALLOWS_INITIALIZED)) {
		return -EALREADY;
	}

	callback = cb;
	return BT_ATT_ERR_SUCCESS;
}

int bt_hallows_handles_assign(struct bt_gatt_dm *dm,
			      struct bt_hallows_client *hallows)
{
	const struct bt_gatt_dm_attr *gatt_service_attr =
			bt_gatt_dm_service_get(dm);
	const struct bt_gatt_service_val *gatt_service =
			bt_gatt_dm_attr_service_val(gatt_service_attr);
	const struct bt_gatt_dm_attr *gatt_chrc;
	const struct bt_gatt_dm_attr *gatt_desc;

	if (bt_uuid_cmp(gatt_service->uuid, BT_UUID_HALLOWS)) {
		return -ENOTSUP;
	}

	LOG_DBG("Getting handles from Hallows service.");

	gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_HALLOWS_CHAR);
	if (!gatt_chrc) {
		LOG_ERR("Missing Hallows characteristic.");
		return -EINVAL;
	}

	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc,
					    BT_UUID_HALLOWS_CHAR);
	if (!gatt_desc) {
		LOG_ERR("Missing Hallows characteristic value descriptor.");
		return -EINVAL;
	}

	LOG_DBG("Found handle for Hallows characteristic.");
	hallows->handle                = gatt_desc->handle;
	hallows->hallows_params.func   = received_hallows_response;
	hallows->hallows_params.handle = hallows->handle;
	hallows->conn                  = bt_gatt_dm_conn_get(dm);
	return 0;
}

int bt_hallows_send(struct bt_hallows_client *hallows,
		       const void *data, uint16_t len)
{
	int err;

	if (atomic_test_and_set_bit(&hallows->state, HALLOWS_ASYNC_WRITE_PENDING)) {
		return -EALREADY;
	}

	LOG_DBG("Sending Hallows request, data %p length %u", data, len);

	hallows->hallows_params.offset = 0;
	hallows->hallows_params.data   = data;
	hallows->hallows_params.length = len;

	err = bt_gatt_write(hallows->conn, &hallows->hallows_params);
	if (err) {
		LOG_ERR("Send Hallows request failed (err %d)", err);
		atomic_clear_bit(&hallows->state, HALLOWS_ASYNC_WRITE_PENDING);
	}
	return err;
}

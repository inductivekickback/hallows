#ifndef BT_HALLOWS_CLIENT_H_
#define BT_HALLOWS_CLIENT_H_

/**
 * @file
 * @defgroup bt_hallows_c Bluetooth LE GATT Hallows client API
 * @{
 * @brief API for the Bluetooth LE GATT Hallows client
 */

#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/gatt_dm.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Hallows client callback structure */
struct bt_hallows_client_cb {
	/** @brief Hallows received callback
	 *
	 * This function confirms that the peripheral received the write.
	 *
	 * @param[in] err 0 if the operation was successful.
	 */
	void (*sent)(uint8_t err);
};

/** @brief Hallows client structure */
struct bt_hallows_client {
	uint16_t handle;
	struct bt_gatt_write_params hallows_params;
	struct bt_conn *conn;
	atomic_t state;
};

/** @brief Initialize the GATT hallows client.
 *
 *  @param[in] hallows Hallows client instance
 *  @param[in] cb Callbacks
 *
 *  @retval 0 If the operation was successful
 *            Otherwise, a negative error code is returned.
 *  @retval (-EINVAL) Special error code used when the input
 *          parameters are invalid.
 *  @retval (-EALREADY) Special error code used when the hallows
 *          client has been initialed.
 */
int bt_hallows_client_init(struct bt_hallows_client *hallows,
			   const struct bt_hallows_client_cb *cb);

/** @brief Assign handles to the hallows client instance.
 *
 *  This function should be called when a link with a peer has been established,
 *  to associate the link to this instance of the module. This makes it
 *  possible to handle several links and associate each link to a particular
 *  instance of this module. The GATT attribute handles are provided by the
 *  GATT Discovery Manager.
 *
 *  @param[in] dm Discovery object
 *  @param[in,out] hallows Hallows client instance
 *
 *  @retval 0 If the operation was successful
 *            Otherwise, a negative error code is returned.
 *  @retval (-ENOTSUP) Special error code used when the UUID
 *          of the service does not match the expected UUID.
 *  @retval (-EINVAL) Special error code used when the UUID
 *          characteristic or value descriptor not found.
 */
int bt_hallows_handles_assign(struct bt_gatt_dm *dm, struct bt_hallows_client *hallows);

/** @brief Write data to the server.
 *
 *  @param[in] hallows Hallows client instance
 *  @param[in] data Data
 *  @param[in] len Data length
 *
 *  @retval 0 If the operation was successful
 *            Otherwise, a negative error code is returned.
 *  @retval (-EALREADY) Special error code used when the asynchronous
 *          request is waiting for a response.
 */
int bt_hallows_send(struct bt_hallows_client *hallows, const void *data, uint16_t len);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* BT_HALLOWS_CLIENT_H_ */

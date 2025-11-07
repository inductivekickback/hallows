#ifndef BT_HALLOWS_H_
#define BT_HALLOWS_H_

/**
 * @file
 * @defgroup bt_hallows Bluetooth LE GATT Hallows Service API
 * @{
 * @brief API for the Bluetooth LE GATT Hallows Service
 */

#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/gatt_dm.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Hallows callback structure */
struct bt_hallows_cb {
    /** @brief Hallows received callback
     *
     * This function is called when a GATT write request has been received
     * by the Hallows characteristic.
     *
     * @param[in] char Candy dispenser index as ASCII integer {'0', '1', '2', '3'}
     */
    void (*dispense_cb)(char index);
};

/** @brief Hallows structure */
struct bt_hallows {
    /** Characteristic handle */
    uint16_t handle;
    struct bt_conn *conn;
    atomic_t state;
};

/** @brief Hallows Service UUID. */

#define BT_UUID_HALLOWS_VAL \
    BT_UUID_128_ENCODE(0xc9fa6d01, 0x0f81, 0x403f, 0xb83d, 0xed9df96095d0)

#define BT_UUID_HALLOWS BT_UUID_DECLARE_128(BT_UUID_HALLOWS_VAL)

#define BT_UUID_HALLOWS_CHAR_VAL \
    BT_UUID_128_ENCODE(0xc9fa6d02, 0x0f81, 0x403f, 0xb83d, 0xed9df96095d0)

/** @brief UUID of the Hallows Characteristic. **/
#define BT_UUID_HALLOWS_CHAR BT_UUID_DECLARE_128(BT_UUID_HALLOWS_CHAR_VAL)

/** @brief Initialize the GATT Hallows service.
 *
 *  @param[in] hallows Hallows service instance
 *  @param[in] cb Callback
 *
 *  @retval 0 If the operation was successful
 *            Otherwise, a negative error code is returned
 *  @retval (-EINVAL) Invalid param
 *  @retval (-EALREADY) Service has already been initialized
 */
int bt_hallows_init(struct bt_hallows *hallows,
            const struct bt_hallows_cb *cb);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* BT_HALLOWS_H_ */

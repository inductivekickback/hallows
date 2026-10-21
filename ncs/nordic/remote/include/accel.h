/** @file
 *  @brief Functions for interfacing with the low-power accelerometer
 */

#ifndef ACCEL_H_
#define ACCEL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/drivers/sensor.h>


int motion_init(sensor_trigger_handler_t motion_cb);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ACCEL_H_ */

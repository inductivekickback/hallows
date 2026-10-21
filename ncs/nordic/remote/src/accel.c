#include "accel.h"


static struct sensor_trigger trig = {
    .type = SENSOR_TRIG_DELTA,
    .chan = SENSOR_CHAN_ACCEL_XYZ,
};

int motion_init(sensor_trigger_handler_t motion_cb)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(lis2dh12));
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }

    /* CTRL_REG2: HPIS1 - high-pass filter data for the INT1 any-motion interrupt */
    struct sensor_value hp = { .val1 = BIT(0) };
    sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_CONFIGURATION, &hp);

    struct sensor_value th  = { .val1 = 2, .val2 = 0 };  /* m/s^2 */
    struct sensor_value dur = { .val1 = 1, .val2 = 0 };  /* in samples */

    sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_TH,  &th);
    sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_DUR, &dur);

    return sensor_trigger_set(dev, &trig, motion_cb);
}

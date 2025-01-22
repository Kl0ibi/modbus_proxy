#include "huawei_modbus_converter.h"
#include "nrgkick_modbus_converter.h"

void solar_logger_post_data(huawei_values_t *huawei_data, nrgkick_values_t *nrgkick_data, bool force);
void solar_logger_init();
void solar_logger_deinit();

#ifndef HUAWEI_MODBUS_CONVERTER_H
#define HUAWEI_MODBUS_CONVERTER_H

#include <stdint.h>
#include <stdbool.h>
#include "system.h"


void huawei_get_info(pv_info_t *info);
void huawei_get_values(pv_values_t *values);

#endif

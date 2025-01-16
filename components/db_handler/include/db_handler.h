#include <stddef.h>
#include <stdint.h>
#include "huawei_modbus_converter.h"

#define DB_HANDLER_OK 0
#define DB_HANDLER_GENERAL_ERROR 1


uint8_t db_handler_send_solar_values(char* formatted_values, size_t formatted_values_len);

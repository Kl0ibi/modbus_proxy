#include "system.h"
#include "modbus_tcp_poll.h"
#include "can_ez3_modbus_converter.h"
#include <math.h>


void can_ez3_get_values(can_ez3_values_t *values) {
    int16_t boiler_temp;
    int16_t sl_temp;

    modbus_tcp_poll_get_client_data(CAN_EZ3, 1, 1, true, (uint8_t *)&boiler_temp);
    if (isnan((double)boiler_temp)) {
        values->boiler_temp = 0;
    }
    else {
        values->boiler_temp = (float)(boiler_temp) / 10;
    }
    modbus_tcp_poll_get_client_data(CAN_EZ3, 2, 1, true, (uint8_t *)&values->sl_power);
    if (isnan((double)values->sl_power)) {
        values->sl_power = 0;
    }
    modbus_tcp_poll_get_client_data(CAN_EZ3, 3, 1, true, (uint8_t *)&sl_temp);
    if (isnan((double)sl_temp)) {
        values->sl_temp = 0;
    }
    else {
        values->sl_temp = (float)(sl_temp) / 10;
    }
    modbus_tcp_poll_get_client_data(CAN_EZ3, 4, 1, true, (uint8_t *)&values->sl_status);
    if (isnan((double)values->sl_status)) {
        values->sl_status = 0;
    }
}

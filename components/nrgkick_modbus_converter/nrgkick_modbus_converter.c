#include <stdio.h>
#include "system.h"
#include "modbus_tcp_poll.h"
#include "nrgkick_modbus_converter.h"
#include <math.h>


void nrgkick_get_values(nrgkick_values_t *values) {
    int32_t charging_power;
    uint16_t current[3];
    int16_t housing_temp;
    int16_t connector_temp[3];
    uint16_t max_current_to_ev;
    uint16_t user_set_current;

    modbus_tcp_poll_get_client_data(NRGKICK, 194, 1, false, (uint8_t *)&user_set_current);
    if (isnan((double)user_set_current)) {
        values->user_set_current = 0;
    }
    else {
        values->user_set_current = (float)(user_set_current) / 10;
    }
    modbus_tcp_poll_get_client_data(NRGKICK, 203, 2, false, (uint8_t *)&values->charged_energy);
    if (isnan((double)values->charged_energy)) {
        values->charged_energy = 0;
    }
    modbus_tcp_poll_get_client_data(NRGKICK, 206, 1, false, (uint8_t *)&max_current_to_ev);
    if (isnan((double)max_current_to_ev)) {
        values->max_current_to_ev = 0;
    }
    else {
        values->max_current_to_ev = (float)(max_current_to_ev) / 10;
    }
    modbus_tcp_poll_get_client_data(NRGKICK, 210, 2, false, (uint8_t *)&charging_power);
    if (isnan((double)charging_power)) {
        values->charging_power = 0;
    }
    else {
        values->charging_power = (float)(charging_power) / 1000;
    }
    for (uint8_t i = 0; i < 3; i++) {
        modbus_tcp_poll_get_client_data(NRGKICK, 220 + i, 1, false, (uint8_t *)&current[i]);
        if (isnan((double)current[i])) {
            values->current[i] = 0;
        }
        else {
            values->current[i] = (float)(current[i]) / 1000;
        }
        modbus_tcp_poll_get_client_data(NRGKICK, 259 + i, 1, false, (uint8_t *)&connector_temp[i]);
        if (isnan((double)connector_temp[i])) {
            values->connector_temp[i] = 0;
        }
        else {
            values->connector_temp[i] = (float)(connector_temp[i]) / 100;
        }
    }
    modbus_tcp_poll_get_client_data(NRGKICK, 251, 1, false, (uint8_t *)&values->cp_status);
    if (isnan((double)values->cp_status)) {
        values->cp_status = 0;
    }
    modbus_tcp_poll_get_client_data(NRGKICK, 253, 1, false, (uint8_t *)&values->switched_relais);
    if (isnan((double)values->switched_relais)) {
        values->switched_relais = 0;
    }
    modbus_tcp_poll_get_client_data(NRGKICK, 256, 1, false, (uint8_t *)&values->warning_code);
    if (isnan((double)values->warning_code)) {
        values->warning_code = 0;
    }
    modbus_tcp_poll_get_client_data(NRGKICK, 257, 1, false, (uint8_t *)&values->error_code);
    if (isnan((double)values->error_code)) {
        values->error_code = 0;
    }
    modbus_tcp_poll_get_client_data(NRGKICK, 258, 1, false, (uint8_t *)&housing_temp);
    if (isnan((double)housing_temp)) {
        values->housing_temp = 0;
    }
    else {
        values->housing_temp = (float)(housing_temp) / 100;
    }
}

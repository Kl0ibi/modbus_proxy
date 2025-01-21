#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "system.h"
#include "modbus_tcp_poll.h"
#include "huawei_modbus_converter.h"


typedef struct __attribute__((packed, aligned(2))){
    int32_t power_real_l3;
    int32_t power_real_l2;
    int32_t power_real_l1;
    int32_t l3_l1_line_voltage;
    int32_t l2_l3_line_voltage;
    int32_t l1_l2_line_voltage;
    uint16_t meter_type;
    int32_t energy_reactive_cons;
    int32_t energy_real_cons;
    int32_t energy_real_prod;
    int16_t grid_freq;
    int16_t power_sf;
    int32_t total_reactive_power;
    int32_t total_real_power;
    int32_t current_l3;
    int32_t current_l2;
    int32_t current_l1;
    int32_t voltage_l3;
    int32_t voltage_l2;
    int32_t voltage_l1;
} huawei_em_request_t;


static char *huawei_battery_product_mode_to_string(uint16_t product) {
	switch (product) {
		case 1:
			return "LG-RESU";
		case 2:
			return "LUNA2000";
		default:
			return "-";
	}
}


static char *huawei_em_type_to_string(uint16_t type) {
	switch (type) {
		case 0:
			return "Single Phase";
		case 1:
			return "Three Phase";
		default:
			return "-";
	}
}


void huawei_get_values(huawei_values_t *values) {
    int32_t temp_pv_dc_w;
	uint16_t temp_bat_soc;
	uint16_t temp_bat_working_mode;
    huawei_em_request_t em;

    modbus_tcp_poll_get_client_data(HUAWEI, 32080, 2, true, (uint8_t *)&values->inverter.inv_ac_w);
    if (isnan((double)values->inverter.inv_ac_w)) {
        values->inverter.inv_ac_w = 0;
    }
    modbus_tcp_poll_get_client_data(HUAWEI, 32106, 2, true, (uint8_t *)&values->inverter.total_pv_energy_wh);
    if (isnan((double)values->inverter.total_pv_energy_wh)) {
        values->inverter.total_pv_energy_wh = 0;
    }
    else {
        values->inverter.total_pv_energy_wh *= 100;
    }
    modbus_tcp_poll_get_client_data(HUAWEI, 32114, 2, true, (uint8_t *)&values->inverter.daily_pv_energy_wh);
    if (isnan((double)values->inverter.daily_pv_energy_wh)) {
        values->inverter.daily_pv_energy_wh = 0;
    }
    else {
        values->inverter.daily_pv_energy_wh *= 10;
    }
    modbus_tcp_poll_get_client_data(HUAWEI, 37765, 2, true, (uint8_t *)&values->battery.battery_power_w);
    if (isnan((double)values->battery.battery_power_w)) {
        values->battery.battery_power_w = 0;
    }
    else {
        values->battery.battery_power_w = (-values->battery.battery_power_w); // huawei inverted
    }
    modbus_tcp_poll_get_client_data(HUAWEI, 37760, 1, true, (uint8_t *)&temp_bat_soc);
    if (isnan((double)temp_bat_soc)) {
        values->battery.battery_soc = 0;
    }
    else {
        values->battery.battery_soc = (float)(temp_bat_soc) / 10;
    }
    modbus_tcp_poll_get_client_data(HUAWEI, 47086, 1, true, (uint8_t *)&temp_bat_working_mode);
    if (isnan((double)temp_bat_working_mode)) {
        values->battery.battery_working_mode = 0;
    }
    temp_pv_dc_w = values->inverter.inv_ac_w - values->battery.battery_power_w;
    values->inverter.pv_dc_w = temp_pv_dc_w < 0 ? 0 : temp_pv_dc_w;
    modbus_tcp_poll_get_client_data(HUAWEI, 37101, 37, true, (uint8_t *)&em);
    if (!em.power_sf) {
        em.power_sf = 1;
    }
    values->energy_meter.p_grid_w = (-em.total_real_power); // huawei is inverted
    values->energy_meter.grid_freq_hz = (float)(em.grid_freq) / 100;
    values->energy_meter.voltage[0] = (float)(em.voltage_l1) / 10;
    values->energy_meter.power_real[0] = -(float)(em.power_real_l1);
    values->energy_meter.current[0] = -(float)(em.current_l1) / 100;
    values->energy_meter.power_apparent[0] = -(float)((float)em.power_real_l1 / (float)em.power_sf);
    if (em.meter_type) {
        values->energy_meter.power_real[1] = -(float)em.power_real_l2;
        values->energy_meter.power_real[2] = -(float)em.power_real_l3;
        values->energy_meter.current[1] = -(float)(em.current_l2) / 100;
        values->energy_meter.current[2] = -(float)(em.current_l3) / 100;
        values->energy_meter.voltage[1] = (float)(em.voltage_l2) / 10;
        values->energy_meter.voltage[2] = (float)(em.voltage_l1) / 10;
        values->energy_meter.power_apparent[1] = -(float)((float)em.power_real_l2 / (float)em.power_sf);
        values->energy_meter.power_apparent[2] = -(float)((float)em.power_real_l3 / (float)em.power_sf);
    }
    else {
        for (uint8_t i = 1; i < 3; i++) {
            values->energy_meter.current[i] = 0;
            values->energy_meter.voltage[i] = 0;
            values->energy_meter.power_real[i] = 0;
            values->energy_meter.power_apparent[i] = 0;
        }
    }
    values->energy_meter.energy_real_cons_wh = em.energy_real_cons * 10;
    values->energy_meter.energy_real_prod_wh = em.energy_real_prod * 10;
    if (em.energy_reactive_cons > 0) {
        values->energy_meter.energy_apparent_cons_vah = (uint64_t)sqrt((double)(values->energy_meter.energy_real_cons_wh * values->energy_meter.energy_real_cons_wh) + (em.energy_reactive_cons * 10 * em.energy_reactive_cons * 10));
        values->energy_meter.energy_apparent_prod_vah = 0;
    }
    else {
        values->energy_meter.energy_apparent_prod_vah = (uint64_t)sqrt((double)(values->energy_meter.energy_real_cons_wh * values->energy_meter.energy_real_cons_wh) + (em.energy_reactive_cons * 10 * em.energy_reactive_cons * 10));
        values->energy_meter.energy_apparent_cons_vah = 0;
    }
	values->energy_meter.p_load_w = values->inverter.inv_ac_w + values->energy_meter.p_grid_w;
}


void huawei_get_info(huawei_info_t *info) {
	char *inv_unique_id = NULL;
	uint16_t inv_unique_id_len;
	char *inv_model = NULL;
	uint16_t inv_model_len;
	uint16_t bat_status;
    char *bat_unique_id;
    uint16_t bat_unique_id_len;
	uint16_t bat_product_model;
	uint16_t meter_status;
	uint16_t meter_type;

    modbus_tcp_poll_get_client_data_str(HUAWEI, 30000, 15, false, &inv_model, &inv_model_len);
    if (inv_model_len > 0 && inv_model[0] != '\0') {
        memcpy(info->inverter.model, inv_model, sizeof(info->inverter.model));
    }
    else {
        info->inverter.model[0] = '\0';
    }
    FREE_MEM(inv_model);
    modbus_tcp_poll_get_client_data_str(HUAWEI, 30015, 10, false, &inv_unique_id, &inv_unique_id_len);
    if(inv_unique_id_len > 0 && inv_unique_id[0] != '\0') {
        memcpy(info->inverter.unique_id, inv_unique_id, sizeof(info->inverter.unique_id));
    }
    else {
        info->inverter.unique_id[0] = '\0';
    }
    FREE_MEM(inv_unique_id);
    modbus_tcp_poll_get_client_data(HUAWEI, 30073, 2, true, (uint8_t *)&info->inverter.max_power_ac_w);
    modbus_tcp_poll_get_client_data(HUAWEI, 37125 , 1, true, (uint8_t *)&meter_type);
    memcpy(info->energy_meter.model, huawei_em_type_to_string(meter_type), sizeof(info->energy_meter.model));
    uint8_t model_len = 0;
    uint8_t unique_len = 0;
    modbus_tcp_poll_get_client_data(HUAWEI, 37000, 1, true, (uint8_t *)&bat_status);
    if (bat_status != 0) {
        modbus_tcp_poll_get_client_data(HUAWEI, 47000, 1, true, (uint8_t *)&bat_product_model);
        model_len = snprintf(info->battery.model, sizeof(info->battery.model), "%s", huawei_battery_product_mode_to_string(bat_product_model));
        modbus_tcp_poll_get_client_data_str(HUAWEI, 37052, 10, false, &bat_unique_id, &bat_unique_id_len);
        if (bat_unique_id_len > 0 && bat_unique_id[0] != '\0') { // bat1
            memcpy(info->battery.unique_id, bat_unique_id, sizeof(info->battery.unique_id));
            unique_len = bat_unique_id_len;
        }
        FREE_MEM(bat_unique_id);
    }
    modbus_tcp_poll_get_client_data(HUAWEI, 37741, 1, true, (uint8_t *)&bat_status);
    if (bat_status != 0) { // bat2
        modbus_tcp_poll_get_client_data(HUAWEI, 47089, 1, true, (uint8_t *)&bat_product_model);
        model_len = snprintf(info->battery.model + model_len, sizeof(info->battery.model), "%s%s", model_len ? " + ": "", huawei_battery_product_mode_to_string(bat_product_model));
        if (unique_len == 0) {
            modbus_tcp_poll_get_client_data_str(HUAWEI, 37700, 10, false, &bat_unique_id, &bat_unique_id_len);
            if (bat_unique_id_len > 0 && bat_unique_id[0] != '\0') {
                memcpy(info->battery.unique_id, bat_unique_id, sizeof(info->battery.unique_id));
                unique_len = bat_unique_id_len;
            }
        }
        FREE_MEM(bat_unique_id);
    }
    if (model_len == 0) {
        info->battery.model[0] = '\0';
    }
    if (unique_len == 0) {
        info->battery.unique_id[0] = '\0';
    }
}

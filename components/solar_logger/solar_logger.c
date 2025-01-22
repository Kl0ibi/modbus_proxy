#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "solar_logger.h"
#include "db_handler.h" 
#include "system.h"
#include "logging.h"


#define MAX_AMOUNT_OF_CACHED_DATA 12352
#define DEBUG_POST_CHUNK_SIZE 30
#define MAX_UPLOAD_TIME_US (4000000)


static const char *TAG = "solar_logger";


typedef struct __attribute__((packed, aligned(1))) {
	uint16_t num_of_cached_values;
	uint16_t oldest_ts_index;
	uint32_t oldest_ts;
	uint32_t newest_ts;
} cached_solar_values_info_t;


typedef struct __attribute__((packed, aligned(1))) test{
	int32_t p_inv_ac :19;
	int32_t p_grid :19;
	int32_t p_load :19;
	int32_t p_bat :19;
	uint32_t cp_status :3;
	uint32_t uploaded :1;
	uint16_t nrg_i_l1;
	uint16_t nrg_i_l2;
	uint16_t nrg_i_l3;
	int16_t p_nrgkick;

	int32_t i_l1 :20;
	uint32_t u_l1 :12;
	int32_t i_l2 :20;
	uint32_t u_l2 :12;
	int32_t i_l3 :20;
	uint32_t u_l3 :12;
	uint32_t ts;

	int32_t p_l1 :22;
	int32_t p_l2 :22;
	int32_t p_l3 :22;
	int32_t s_l1 :22;
	int32_t s_l2 :22;
	int32_t s_l3 :22;
    int32_t freq :22;
	uint32_t p_pv :18;
	uint32_t bat_soc_f10 :10;
    uint32_t energy_prod_day;
    // 2bits free

    uint64_t energy_prod_wh;
    uint64_t energy_exp_wh;
    uint64_t energy_imp_wh;

    uint8_t warning_code;
    uint8_t error_code;
    int16_t connector_temp_l1;
    int16_t connector_temp_l2;
    int16_t connector_temp_l3;
    uint16_t max_current_to_ev;
    uint16_t user_set_current;
    uint32_t charged_energy;
    int16_t housing_temp;
    uint8_t switched_relais :4;
    // 4 bits free
} cached_solar_values_t;


static uint16_t cache_index = 0;
static bool cache_overflow = false;
static uint16_t upload_index = 0;
static cached_solar_values_t *cached_values = NULL;
static bool send_live_mode = false;
static cached_solar_values_info_t cached_values_info;


void solar_logger_get_cached_values_info(cached_solar_values_info_t *info) {
	*info = cached_values_info;
}


cached_solar_values_t* solar_logger_get_all_cached_values() {
	return cached_values;
}


static void update_cached_values(huawei_values_t *values, nrgkick_values_t *nrgkick_values, time_t ts) {
    cached_values[cache_index].ts = (uint32_t)ts;
    cached_values[cache_index].p_pv = values->inverter.pv_dc_w > MAX_UINTN(18) ? MAX_UINTN(18) : values->inverter.pv_dc_w;
    cached_values[cache_index].energy_prod_day = values->inverter.daily_pv_energy_wh;
    cached_values[cache_index].energy_prod_wh = values->inverter.total_pv_energy_wh;
    cached_values[cache_index].p_grid = values->energy_meter.p_grid_w >= 0 ? (values->energy_meter.p_grid_w > MAX_INTN(19) ? MAX_INTN(19) : values->energy_meter.p_grid_w) : (values->energy_meter.p_grid_w < MIN_INTN(19) ? MIN_INTN(19) : values->energy_meter.p_grid_w);
    cached_values[cache_index].p_load = values->energy_meter.p_load_w >= 0 ? (values->energy_meter.p_load_w > MAX_INTN(19) ? MAX_INTN(19) : values->energy_meter.p_load_w) : (values->energy_meter.p_load_w < MIN_INTN(19) ? MIN_INTN(19) : values->energy_meter.p_load_w);
    cached_values[cache_index].energy_exp_wh = values->energy_meter.energy_real_prod_wh;
    cached_values[cache_index].energy_imp_wh = values->energy_meter.energy_real_cons_wh;
    cached_values[cache_index].freq = values->energy_meter.grid_freq_hz >= 0 ? ((int32_t)(values->energy_meter.grid_freq_hz * 100) > MAX_INTN(20) ? MAX_INTN(20) : (int32_t)(values->energy_meter.grid_freq_hz * 100)) : ((int32_t)(values->energy_meter.grid_freq_hz * 100) < MIN_INTN(20) ? MIN_INTN(20) : (int32_t)(values->energy_meter.grid_freq_hz * 100));
    cached_values[cache_index].i_l1 = values->energy_meter.current[0] >= 0 ? ((int32_t)(values->energy_meter.current[0] * 100) > MAX_INTN(20) ? MAX_INTN(20) : (int32_t)(values->energy_meter.current[0] * 100)) : ((int32_t)(values->energy_meter.current[0] * 100) < MIN_INTN(20) ? MIN_INTN(20) : (int32_t)(values->energy_meter.current[0] * 100));
    cached_values[cache_index].i_l2 = values->energy_meter.current[1] >= 0 ? ((int32_t)(values->energy_meter.current[1] * 100) > MAX_INTN(20) ? MAX_INTN(20) : (int32_t)(values->energy_meter.current[1] * 100)) : ((int32_t)(values->energy_meter.current[1] * 100) < MIN_INTN(20) ? MIN_INTN(20) : (int32_t)(values->energy_meter.current[1] * 100));
    cached_values[cache_index].i_l3 = values->energy_meter.current[2] >= 0 ? ((int32_t)(values->energy_meter.current[2] * 100) > MAX_INTN(20) ? MAX_INTN(20) : (int32_t)(values->energy_meter.current[2] * 100)) : ((int32_t)(values->energy_meter.current[2] * 100) < MIN_INTN(20) ? MIN_INTN(20) : (int32_t)(values->energy_meter.current[2] * 100));
    cached_values[cache_index].u_l1 = values->energy_meter.voltage[0] >= 0 ? ((uint32_t)(values->energy_meter.voltage[0] * 10) > MAX_UINTN(12) ? MAX_UINTN(12) : (uint32_t)(values->energy_meter.voltage[0] * 10)) : (0);
    cached_values[cache_index].u_l2 = values->energy_meter.voltage[1] >= 0 ? ((uint32_t)(values->energy_meter.voltage[1] * 10) > MAX_UINTN(12) ? MAX_UINTN(12) : (uint32_t)(values->energy_meter.voltage[1] * 10)) : (0);
    cached_values[cache_index].u_l3 = values->energy_meter.voltage[2] >= 0 ? ((uint32_t)(values->energy_meter.voltage[2] * 10) > MAX_UINTN(12) ? MAX_UINTN(12) : (uint32_t)(values->energy_meter.voltage[2] * 10)) : (0);
    cached_values[cache_index].p_l1 = values->energy_meter.power_real[0] >= 0 ? ((int32_t)(values->energy_meter.power_real[0] * 10) > MAX_INTN(22) ? MAX_INTN(22) : (int32_t)(values->energy_meter.power_real[0] * 10)) : ((int32_t)(values->energy_meter.power_real[0] * 10) < MIN_INTN(22) ? MIN_INTN(22) : (int32_t)(values->energy_meter.power_real[0] * 10));
    cached_values[cache_index].p_l2 = values->energy_meter.power_real[1] >= 0 ? ((int32_t)(values->energy_meter.power_real[1] * 10) > MAX_INTN(22) ? MAX_INTN(22) : (int32_t)(values->energy_meter.power_real[1] * 10)) : ((int32_t)(values->energy_meter.power_real[1] * 10) < MIN_INTN(22) ? MIN_INTN(22) : (int32_t)(values->energy_meter.power_real[1] * 10));
    cached_values[cache_index].p_l3 = values->energy_meter.power_real[2] >= 0 ? ((int32_t)(values->energy_meter.power_real[2] * 10) > MAX_INTN(22) ? MAX_INTN(22) : (int32_t)(values->energy_meter.power_real[2] * 10)) : ((int32_t)(values->energy_meter.power_real[2] * 10) < MIN_INTN(22) ? MIN_INTN(22) : (int32_t)(values->energy_meter.power_real[2] * 10));
    cached_values[cache_index].s_l1 = values->energy_meter.power_apparent[0] >= 0 ? ((int32_t)(values->energy_meter.power_apparent[0] * 10) > MAX_INTN(22) ? MAX_INTN(22) : (int32_t)(values->energy_meter.power_apparent[0] * 10)) : ((int32_t)(values->energy_meter.power_apparent[0] * 10) < MIN_INTN(22) ? MIN_INTN(22) : (int32_t)(values->energy_meter.power_apparent[0] * 10));
    cached_values[cache_index].s_l2 = values->energy_meter.power_apparent[1] >= 0 ? ((int32_t)(values->energy_meter.power_apparent[1] * 10) > MAX_INTN(22) ? MAX_INTN(22) : (int32_t)(values->energy_meter.power_apparent[1] * 10)) : ((int32_t)(values->energy_meter.power_apparent[1] * 10) < MIN_INTN(22) ? MIN_INTN(22) : (int32_t)(values->energy_meter.power_apparent[1] * 10));
    cached_values[cache_index].s_l3 = values->energy_meter.power_apparent[2] >= 0 ? ((int32_t)(values->energy_meter.power_apparent[2] * 10) > MAX_INTN(22) ? MAX_INTN(22) : (int32_t)(values->energy_meter.power_apparent[2] * 10)) : ((int32_t)(values->energy_meter.power_apparent[2] * 10) < MIN_INTN(22) ? MIN_INTN(22) : (int32_t)(values->energy_meter.power_apparent[2] * 10));
    cached_values[cache_index].bat_soc_f10 = (uint16_t)(values->battery.battery_soc <= 0 ? 0 : (values->battery.battery_soc <= 100 ? (values->battery.battery_soc * 10) : 1000));
    cached_values[cache_index].p_bat = values->battery.battery_power_w >= 0 ? (values->battery.battery_power_w  > MAX_INTN(19) ? MAX_INTN(19) : values->battery.battery_power_w) : (values->battery.battery_power_w < MIN_INTN(19) ? MIN_INTN(19) : values->battery.battery_power_w);
 
    cached_values[cache_index].switched_relais = nrgkick_values->switched_relais;
    cached_values[cache_index].cp_status = nrgkick_values->cp_status;
    cached_values[cache_index].charged_energy = nrgkick_values->charged_energy;
    cached_values[cache_index].warning_code = nrgkick_values->warning_code;
    cached_values[cache_index].error_code = nrgkick_values->error_code;

    cached_values[cache_index].connector_temp_l1 = nrgkick_values->connector_temp[0] * 100;
    cached_values[cache_index].connector_temp_l2 = nrgkick_values->connector_temp[1] * 100;
    cached_values[cache_index].connector_temp_l3 = nrgkick_values->connector_temp[2] * 100;
    cached_values[cache_index].nrg_i_l1 = nrgkick_values->current[0] * 1000;
    cached_values[cache_index].nrg_i_l2 = nrgkick_values->current[1] * 1000;
    cached_values[cache_index].nrg_i_l3 = nrgkick_values->current[2] * 1000;
    cached_values[cache_index].p_nrgkick = nrgkick_values->charging_power * 1000;
    cached_values[cache_index].housing_temp = nrgkick_values->housing_temp * 100;
    cached_values[cache_index].max_current_to_ev = nrgkick_values->user_set_current * 10;
    cached_values[cache_index].user_set_current = nrgkick_values->user_set_current * 10;

	cached_values[cache_index].uploaded = false;

	cache_index++;
	if (cache_index >= MAX_AMOUNT_OF_CACHED_DATA) {
		cache_index = 0;
		cache_overflow = true;
	}
	if (cache_overflow) {
		cached_values_info.num_of_cached_values = MAX_AMOUNT_OF_CACHED_DATA;
		cached_values_info.oldest_ts_index = cache_index;
		if ((cached_values_info.oldest_ts_index > upload_index && !cached_values[cached_values_info.oldest_ts_index].uploaded) || (cached_values_info.oldest_ts_index == 0 &&
		                                                                                                                           upload_index == (MAX_AMOUNT_OF_CACHED_DATA - 1) && !cached_values[cached_values_info.oldest_ts_index].uploaded)) {
			upload_index = cached_values_info.oldest_ts_index;
		}
		if (cache_index == 0) {
			cached_values_info.newest_ts = cached_values[MAX_AMOUNT_OF_CACHED_DATA - 1].ts;
			cached_values_info.oldest_ts = cached_values[0].ts;
		}
		else {
			cached_values_info.newest_ts = cached_values[cache_index - 1].ts;
			cached_values_info.oldest_ts = cached_values[cache_index].ts;
		}
	}
	else {
		cached_values_info.num_of_cached_values = cache_index;
		if (cache_index != 0) {
			cached_values_info.oldest_ts = cached_values[0].ts;
			cached_values_info.newest_ts = cached_values[cache_index - 1].ts;
		}
		else {
			cached_values_info.oldest_ts = 0;
			cached_values_info.newest_ts = 0;
		}
		cached_values_info.oldest_ts_index = 0;
	}
}


void solar_logger_post_data(huawei_values_t *huawei_data, nrgkick_values_t *nrgkick_data, bool force) {
	static uint8_t cached_values_local = 0;
	time_t ts;
	char *api_data;
	char *api_ext;

	if ((ts = time(NULL)) != -1) {
		update_cached_values(huawei_data, nrgkick_data, ts);
		if (send_live_mode) {
			force = true;
		}
		cached_values_local++;
		if (force || cached_values_local >= DEBUG_POST_CHUNK_SIZE) {
			char *data = NULL;
			char **data_strings = NULL;
			uint32_t size;
			send_live_mode = false;
			data_strings = malloc(DEBUG_POST_CHUNK_SIZE * sizeof(char *));

			uint8_t cnt = 0;
			uint64_t upload_timeout = get_time_in_milliseconds() + MAX_UPLOAD_TIME_US;
			while (true) {
				uint16_t temp_upload_index = upload_index + cnt;
				if (temp_upload_index >= MAX_AMOUNT_OF_CACHED_DATA) {
					temp_upload_index -= MAX_AMOUNT_OF_CACHED_DATA;
				}
				if (upload_index == cache_index) {
					if (cached_values[temp_upload_index].uploaded) {
						LOGI(TAG, "All uploaded!");
						break;
					}
				}
                asprintf(&data_strings[cnt],
                    "power_data,source=grid "
                    "p_pv=%d,"
                    "p_inv_ac=%d,"
                    "p_grid=%d,"
                    "p_load=%d,"
                    "p_bat=%d,"
                    "soc=%f,"
                    "freq=%.2f,"
                    "i_l1=%.2f,"
                    "u_l1=%.1f,"
                    "i_l2=%.2f,"
                    "u_l2=%.1f,"
                    "i_l3=%.2f,"
                    "u_l3=%.1f,"
                    "p_l1=%.2f,"
                    "p_l2=%.2f,"
                    "p_l3=%.2f,"
                    "s_l1=%.2f,"
                    "s_l2=%.2f,"
                    "s_l3=%.2f,"
                    "energy_prod_day=%u,"
                    "energy_prod_wh=%lu,"
                    "energy_exp_wh=%lu,"
                    "energy_imp_wh=%lu,"
                    "nrg_i_l1=%.2f,"
                    "nrg_i_l2=%.2f,"
                    "nrg_i_l3=%.2f,"
                    "charging_power=%.3f,"
                    "charged_energy=%u,"
                    "housing_temp=%.2f,"
                    "cp_status=%hhu,"
                    "switched_relais=%hhu,"
                    "warning_code=%hhu,"
                    "error_code=%hhu,"
                    "connector_temp_l1=%.2f,"
                    "connector_temp_l2=%.2f,"
                    "connector_temp_l3=%.2f,"
                    "max_current_to_ev=%.1f,"
                    "user_set_current=%.1f,"
                    "%u000000000\n",
                    cached_values[temp_upload_index].p_pv,
                    cached_values[temp_upload_index].p_inv_ac,
                    cached_values[temp_upload_index].p_grid,
                    cached_values[temp_upload_index].p_load,
                    cached_values[temp_upload_index].p_bat,
                    (float)(cached_values[temp_upload_index].bat_soc_f10) / 10,
                    (float)(cached_values[temp_upload_index].freq) / 100,
                    (float)(cached_values[temp_upload_index].i_l1) / 100,
                    (float)(cached_values[temp_upload_index].u_l1) / 10,
                    (float)(cached_values[temp_upload_index].i_l2) / 100,
                    (float)(cached_values[temp_upload_index].u_l2) / 10,
                    (float)(cached_values[temp_upload_index].i_l3) / 100,
                    (float)(cached_values[temp_upload_index].u_l3) / 10,
                    (float)(cached_values[temp_upload_index].p_l1) / 10,
                    (float)(cached_values[temp_upload_index].p_l2) / 10,
                    (float)(cached_values[temp_upload_index].p_l3) / 10,
                    (float)(cached_values[temp_upload_index].s_l1) / 10,
                    (float)(cached_values[temp_upload_index].s_l2) / 10,
                    (float)(cached_values[temp_upload_index].s_l3) / 10,
                    cached_values[temp_upload_index].energy_prod_day,
                    cached_values[temp_upload_index].energy_prod_wh,
                    cached_values[temp_upload_index].energy_exp_wh,
                    cached_values[temp_upload_index].energy_imp_wh,
                    (float)cached_values[temp_upload_index].nrg_i_l1,
                    (float)cached_values[temp_upload_index].nrg_i_l2,
                    (float)cached_values[temp_upload_index].nrg_i_l3,
                    (float)(cached_values[temp_upload_index].p_nrgkick) / 1000,
                    cached_values[temp_upload_index].charged_energy,
                    (float)(cached_values[temp_upload_index].housing_temp) / 100,
                    cached_values[temp_upload_index].cp_status,
                    cached_values[temp_upload_index].switched_relais,
                    cached_values[temp_upload_index].warning_code,
                    cached_values[temp_upload_index].error_code,
                    (float)(cached_values[temp_upload_index].connector_temp_l1) / 100,
                    (float)(cached_values[temp_upload_index].connector_temp_l2) / 100,
                    (float)(cached_values[temp_upload_index].connector_temp_l3) / 100,
                    (float)(cached_values[temp_upload_index].max_current_to_ev) / 10,
                    (float)(cached_values[temp_upload_index].user_set_current) / 10,
                    cached_values[temp_upload_index].ts
                );
				if (cnt == DEBUG_POST_CHUNK_SIZE - 1 || temp_upload_index == (cache_index == 0 ? (MAX_AMOUNT_OF_CACHED_DATA - 1) : (cache_index - 1))) {
					if (temp_upload_index == (cache_index == 0 ? (MAX_AMOUNT_OF_CACHED_DATA - 1) : (cache_index - 1)) && cnt != DEBUG_POST_CHUNK_SIZE - 1) {
						for (uint8_t x = cnt + 1; x < DEBUG_POST_CHUNK_SIZE; x++) {
							data_strings[x] = NULL;
						}
					}
					size = asprintf(&data, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
					                    data_strings[0] != NULL ? data_strings[0] : "\0", data_strings[1] != NULL ? data_strings[1] : "\0",
					                    data_strings[2] != NULL ? data_strings[2] : "\0", data_strings[3] != NULL ? data_strings[3] : "\0",
					                    data_strings[4] != NULL ? data_strings[4] : "\0", data_strings[5] != NULL ? data_strings[5] : "\0",
					                    data_strings[6] != NULL ? data_strings[6] : "\0", data_strings[7] != NULL ? data_strings[7] : "\0",
					                    data_strings[8] != NULL ? data_strings[8] : "\0", data_strings[9] != NULL ? data_strings[9] : "\0",
					                    data_strings[10] != NULL ? data_strings[10] : "\0", data_strings[11] != NULL ? data_strings[11] : "\0",
					                    data_strings[12] != NULL ? data_strings[12] : "\0", data_strings[13] != NULL ? data_strings[13] : "\0",
					                    data_strings[14] != NULL ? data_strings[14] : "\0", data_strings[15] != NULL ? data_strings[15] : "\0",
					                    data_strings[16] != NULL ? data_strings[16] : "\0", data_strings[17] != NULL ? data_strings[17] : "\0",
					                    data_strings[18] != NULL ? data_strings[18] : "\0", data_strings[19] != NULL ? data_strings[19] : "\0",
					                    data_strings[20] != NULL ? data_strings[20] : "\0", data_strings[21] != NULL ? data_strings[21] : "\0",
					                    data_strings[22] != NULL ? data_strings[22] : "\0", data_strings[23] != NULL ? data_strings[23] : "\0",
					                    data_strings[24] != NULL ? data_strings[24] : "\0", data_strings[25] != NULL ? data_strings[25] : "\0",
					                    data_strings[26] != NULL ? data_strings[26] : "\0", data_strings[27] != NULL ? data_strings[27] : "\0",
					                    data_strings[28] != NULL ? data_strings[28] : "\0", data_strings[29] != NULL ? data_strings[29] : "\0");
					
                    printf("post data: %s\n", data); //TODO: remove
					if (db_handler_send_solar_values(data, size) == DB_HANDLER_OK) {
						LOGI(TAG, "Successfully posted logging data");
						for (uint8_t i = 0; i < (cnt + 1); i++) {
							if ((upload_index + i) < MAX_AMOUNT_OF_CACHED_DATA) {
								cached_values[upload_index + i].uploaded = true;
							}
							else {
								cached_values[(upload_index + i) - MAX_AMOUNT_OF_CACHED_DATA].uploaded = true;
							}
						}
						upload_index += (cnt + 1);
						if (upload_index >= MAX_AMOUNT_OF_CACHED_DATA) {
							upload_index -= MAX_AMOUNT_OF_CACHED_DATA;
						}
						cnt = 0;
						FREE_MEM(data);
						if (upload_index == cache_index) {
							LOGI(TAG, "All uploaded!");
							break;
						}
						if (get_time_in_milliseconds() >= upload_timeout) {
							LOGI(TAG, "Upload time reached timeout --> upload remaining offline data in next cycle");
							send_live_mode = true;
							break;
						}
						for (uint8_t i = 0; i < DEBUG_POST_CHUNK_SIZE; i++) {
							FREE_MEM(data_strings[i]);
						}
					}
					else {
   					LOGE(TAG, "Error during uploading of logging data");
						FREE_MEM(data);
						break;
					}
				}
				else {
					cnt++;
				}
			}

			for (uint8_t i = 0; i < DEBUG_POST_CHUNK_SIZE; i++) {
				FREE_MEM(data_strings[i]);
			}
			FREE_MEM(data_strings);
			exit:
			cached_values_local = 0;
		}
	}
}


void solar_logger_init() {
	if (cached_values == NULL) {
		cached_values = malloc(MAX_AMOUNT_OF_CACHED_DATA * sizeof(cached_solar_values_t));
	}
	upload_index = 0;
	cached_values_info.num_of_cached_values = 0;
}


void solar_logger_deinit() {
	FREE_MEM(cached_values);
	cache_index = 0;
	cache_overflow = false;
	upload_index = 0;
	send_live_mode = false;
	cached_values_info.num_of_cached_values = 0;
}

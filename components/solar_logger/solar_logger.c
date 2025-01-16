#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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


typedef struct __attribute__((packed, aligned(1))) {
	uint32_t p_pv :18;
	int32_t p_grid :19;
	int32_t p_load :19;
	int32_t p_bat :19;
	uint16_t num_phases :2;
	uint32_t cp_status :3;

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
	int32_t p_total :22;
	int32_t s_l1 :22;
	int32_t s_l2 :22;
	int32_t s_l3 :22;

	uint32_t bat_soc_f10 :10;
	uint32_t uploaded :1;
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



static void update_cached_values(cached_solar_values_t *values) {
	memcpy(&cached_values[cache_index], values, sizeof(cached_solar_values_t));
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


void solar_logger_post_data(cached_solar_values_t *post_data, bool force, bool active_trigger) {
	static uint8_t cached_values_local = 0;
	time_t ts;
	char *api_data;
	char *api_ext;

	if ((ts = time(NULL)) != -1) {
		post_data->ts = (uint32_t)ts;
		update_cached_values(post_data);
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
			uint64_t upload_timeout = esp_timer_get_time() + MAX_UPLOAD_TIME_US;
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

				asprintf(&data_strings[cnt],"%s{\"ts\":%lu,\"i_l1\":%.2f,\"i_l2\":%.2f,\"i_l3\":%.2f,\"u_l1\":%.1f,\"u_l2\":%.1f,\"u_l3\":%.1f,\"p_l1\":%.1f,"
				                                "\"p_l2\":%.1f,\"p_l3\":%.1f,\"p_total\":%.1f,\"s_l1\":%.1f,\"s_l2\":%.1f,\"s_l3\":%.1f,\"type_e_lim\":%hhu,"
				                                "\"e_s_total\":%llu,\"e_p_total\":%llu,\"e_period_start\":%llu,\"ts_period_start\":%lu,\"e_period_end\":%llu,\"ts_period_end\":%lu,\"p_nrg_total\":[%d],\"state_sm\":[%hhu],\"i_req\":[%.1f],\"cp_stat\":[%hhu],\"rel_stat\":[%hhu],"
				                                "\"phase_det_status\":[%hhu],\"phase_det_mask\":[%hhu],\"i_l1_nrg\":[%.3f],\"i_l2_nrg\":[%.3f],\"i_l3_nrg\":[%.3f],\"i_n_nrg\":[%.3f],\"u_l1_nrg\":[%.2f],\"u_l2_nrg\":[%.2f],\"u_l3_nrg\":[%.2f]"
				                                "%s%s%s%s%s%s}", cnt != 0 ? "," : "\0",
				             cached_values[temp_upload_index].ts, ((float)(cached_values[temp_upload_index].i_l1) / 100),
				             ((float)(cached_values[temp_upload_index].i_l2) / 100), ((float)(cached_values[temp_upload_index].i_l3) / 100),
				             ((float)(cached_values[temp_upload_index].u_l1) / 10), ((float)(cached_values[temp_upload_index].u_l2) / 10),
				             ((float)(cached_values[temp_upload_index].u_l3) / 10), ((float)(cached_values[temp_upload_index].p_l1) / 10),
				             ((float)(cached_values[temp_upload_index].p_l2) / 10), ((float)(cached_values[temp_upload_index].p_l3) / 10),
				             ((float)(cached_values[temp_upload_index].p_total) / 10), ((float)(cached_values[temp_upload_index].s_l1) / 10),
				             ((float)(cached_values[temp_upload_index].s_l2) / 10), ((float)(cached_values[temp_upload_index].s_l3) / 10),  cached_values[temp_upload_index].use_apparent,
				             cached_values[temp_upload_index].energy_s_total, cached_values[temp_upload_index].energy_p_total,
							 cached_values[temp_upload_index].energy_period_start, cached_values[temp_upload_index].ts_period_start,
                             cached_values[temp_upload_index].max_energy_period_end_real, cached_values[temp_upload_index].ts_period_end,
							 cached_values[temp_upload_index].p_nrgkick, cached_values[temp_upload_index].sm_state,
							 ((float)(cached_values[temp_upload_index].sm_to_main) / 10), cached_values[temp_upload_index].cp_status, cached_values[temp_upload_index].relay_state,
							 cached_values[temp_upload_index].phase_detection_status, cached_values[temp_upload_index].phase_detection_mask, (float)(cached_values[temp_upload_index].nrg_i_l1) / 1000,
							 (float)(cached_values[temp_upload_index].nrg_i_l2) / 1000, (float)(cached_values[temp_upload_index].nrg_i_l3) / 1000,(float)(cached_values[temp_upload_index].nrg_i_n) / 1000,
							 (float)(cached_values[temp_upload_index].nrg_u_l1) / 100, (float)(cached_values[temp_upload_index].nrg_u_l2) / 100, (float)(cached_values[temp_upload_index].nrg_u_l3) / 100,
				             ext_data != NULL ? "," : "\0", ext_data != NULL ? ext_data : "\0",
				             api_data != NULL ? "," : "\0", api_data != NULL ? api_data : "\0",
				             api_ext != NULL ? "," : "\0", api_ext != NULL ? api_ext : "\0");


				if (cnt == DEBUG_POST_CHUNK_SIZE - 1 || temp_upload_index == (cache_index == 0 ? (MAX_AMOUNT_OF_CACHED_DATA - 1) : (cache_index - 1))) {
					if (temp_upload_index == (cache_index == 0 ? (MAX_AMOUNT_OF_CACHED_DATA - 1) : (cache_index - 1)) && cnt != DEBUG_POST_CHUNK_SIZE - 1) {
						for (uint8_t x = cnt + 1; x < DEBUG_POST_CHUNK_SIZE; x++) {
							data_strings[x] = NULL;
						}
					}
					size = asprintf(&data, "{\"data\":[%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s]}",
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
                   
					
					if (db_handler_send_solar_values(NULL) == DB_HANDLER_OK) { // TODO: here add right param
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
						if (esp_timer_get_time() >= upload_timeout) {
							LOGI(TAG, "Upload time reached timeout --> upload remaining offline data in next cycle");
							send_live_mode = true;
							break;
						}
						for (uint8_t i = 0; i < DEBUG_POST_CHUNK_SIZE; i++) {
							FREE_MEM(data_strings[i]);
						}
						vTaskDelay(10);
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


__attribute__((unused)) void solar_logger_trigger_post_request(solar_options_t *options) {
	//solar_logger_post_data(options, NULL, true, false);
}


void solar_logger_init() {
	if (cached_values == NULL) {
		cached_values = nrgsystem_malloc_extr(MAX_AMOUNT_OF_CACHED_DATA * sizeof(solar_cached_values_t));
	}
	ext_cached_values_info.upload_index = 0;
}


void solar_logger_deinit() {
	FREE_MEM(cached_values);
	cache_index = 0;
	cache_overflow = false;
	upload_index = 0;
	send_live_mode = false;
	cached_values_info.num_of_cached_values = 0;
}

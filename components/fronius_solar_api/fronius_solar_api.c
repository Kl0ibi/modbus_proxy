#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <math.h>
#include "logging.h"
#include "cJSON.h"
#include "system.h"

#define FRONIUS_OK (0)
#define FRONIUS_ERROR (1)


const char *TAG = "fronius_api";


static size_t write_callback(void *ptr, size_t size, size_t nmemb, char **data) {
    size_t total_size = size * nmemb;
    
    *data = malloc(total_size + 1);
    if (*data == NULL) {
        LOGE(TAG, "Memory allocation error");
        return 0;
    }
    memcpy(*data, ptr, total_size);
    (*data)[total_size] = '\0';
    
    return total_size;
}

static uint8_t http_get(const char *url, char **response, long *status_code) {
    uint8_t rc;
    CURL *curl;
    CURLcode res;
    char header_buffer[128];
    size_t response_len = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        if ((res = curl_easy_perform(curl))== CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status_code);
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &response_len);
            LOGD(TAG, "HTTP Status Code: %ld", *status_code);
            LOGD(TAG, "Reponse Length: %zu bytes", response_len);
        }
        else {
            LOGE(TAG, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
            rc = FRONIUS_ERROR;
            goto exit;
        }
    }

    rc = FRONIUS_OK;
    exit:
    if (curl) {
        curl_easy_cleanup(curl);
    }
    else {
        curl_global_cleanup();
    }
    return rc;
}


static uint8_t get_ivnerter_realtime_data(pv_values_t *values) {
    uint8_t rc;
    long status_code;
    char *response = NULL;
    cJSON *json = NULL;


    if (http_get("http://localhost:6000/solar_api/v1/GetInverterRealtimeData.cgi?Scope=Device&DeviceId=1&DataCollection=CommonInverterData", &response, &status_code) != FRONIUS_OK) {
        rc = FRONIUS_ERROR;
        goto exit;
    }
    if (status_code != 200) {
        rc = FRONIUS_ERROR;
        goto exit;
    }
    if ((json = cJSON_Parse(response)) != NULL) {
        cJSON *body;
        cJSON *data;
        cJSON *idc;
        cJSON *idc2;
        cJSON *udc;
        cJSON *udc2;
        cJSON *pac;
        cJSON *total_energy;
        cJSON *value;

        if ((body = cJSON_GetObjectItemCaseSensitive(json, "Body")) != NULL) {
            if ((data = cJSON_GetObjectItemCaseSensitive(body, "Data")) != NULL) {
                if ((idc = cJSON_GetObjectItemCaseSensitive(data, "IDC")) == NULL || ((value = cJSON_GetObjectItemCaseSensitive(idc, "Value")) == NULL || !cJSON_IsNumber(value))) {
                    LOGE(TAG, "Error reading IDC"),
                    rc = FRONIUS_ERROR;
                    goto exit;
                }
                values->inverter.pv1_current = value->valueint;
                if ((idc2 = cJSON_GetObjectItemCaseSensitive(data, "IDC2")) == NULL || ((value = cJSON_GetObjectItemCaseSensitive(idc2, "Value")) == NULL || !cJSON_IsNumber(value))) {
                    LOGE(TAG, "Error reading IDC2"),
                    rc = FRONIUS_ERROR;
                    goto exit;
                }
                values->inverter.pv2_current = value->valueint;
                if ((udc = cJSON_GetObjectItemCaseSensitive(data, "UDC")) == NULL || ((value = cJSON_GetObjectItemCaseSensitive(udc, "Value")) == NULL || !cJSON_IsNumber(value))) {
                    LOGE(TAG, "Error reading UDC"),
                    rc = FRONIUS_ERROR;
                    goto exit;
                }
                values->inverter.pv1_voltage = value->valueint;
                if ((udc2 = cJSON_GetObjectItemCaseSensitive(data, "UDC2")) == NULL || ((value = cJSON_GetObjectItemCaseSensitive(udc2, "Value")) == NULL || !cJSON_IsNumber(value))) {
                    LOGE(TAG, "Error reading UDC2"),
                    rc = FRONIUS_ERROR;
                    goto exit;
                }
                values->inverter.pv2_voltage = value->valueint;
                if ((pac = cJSON_GetObjectItemCaseSensitive(data, "PAC")) == NULL || ((value = cJSON_GetObjectItemCaseSensitive(pac, "Value")) == NULL || !cJSON_IsNumber(value))) {
                    LOGE(TAG, "Error reading pac"),
                    rc = FRONIUS_ERROR;
                    goto exit;
                }
                values->inverter.inv_ac_w = value->valueint;
                if ((total_energy = cJSON_GetObjectItemCaseSensitive(data, "TOTAL_ENERGY")) == NULL || ((value = cJSON_GetObjectItemCaseSensitive(total_energy, "Value")) == NULL || !cJSON_IsNumber(value))) {
                    LOGE(TAG, "Error reading total_energy"),
                    rc = FRONIUS_ERROR;
                    goto exit;
                }
                values->inverter.total_pv_energy_wh = value->valueint;
                values->inverter.pv1_power = values->inverter.pv1_voltage * values->inverter.pv1_current;
                values->inverter.pv2_power = values->inverter.pv2_voltage * values->inverter.pv2_current;
                values->inverter.pv_dc_w = values->inverter.pv1_power + values->inverter.pv2_power;
                values->inverter.daily_pv_energy_wh = 0;
                values->inverter.daily_inv_energy_wh = 0;
                values->inverter.total_inv_energy_wh = 0;
                rc = FRONIUS_OK;
                goto exit;
            }
        }
    }

    rc = FRONIUS_ERROR;
    exit:
    if (json) {
        cJSON_free(json);
    }
    FREE_MEM(response);
    return rc;
}


static uint8_t get_meter_realtime_data(pv_values_t *values) {
    uint8_t rc;
    long status_code;
    char *response = NULL;
    cJSON *json = NULL;

    if (http_get("http://localhost:6000/solar_api/v1/GetMeterRealtimeData.fcgi", &response, &status_code) != FRONIUS_OK) {
        rc = FRONIUS_ERROR;
        goto exit;
    }
    if (status_code != 200) {
        rc = FRONIUS_ERROR;
        goto exit;
    }
    if ((json = cJSON_Parse(response)) != NULL) {
        cJSON *body;
        cJSON *data;
        cJSON *id;
        cJSON *grid_power;
        cJSON *current;
        cJSON *voltage;
        cJSON *real;
        cJSON *apparent;
        cJSON *energy_real_cons;
        cJSON *energy_real_prod;
        cJSON *freq;

        if ((body = cJSON_GetObjectItemCaseSensitive(json, "Body")) != NULL) {
            if ((data = cJSON_GetObjectItemCaseSensitive(body, "Data")) != NULL) {
                if ((id = cJSON_GetObjectItemCaseSensitive(data, "0")) != NULL) {
                    if ((grid_power = cJSON_GetObjectItemCaseSensitive(id, "PowerReal_P_Sum")) == NULL) {
                        rc = FRONIUS_ERROR;
                        goto exit;
                    }
                    values->energy_meter.p_grid_w = grid_power->valueint;
                    char key[25];
                    for (uint8_t x = 1; x <= 3; x++) {
                        snprintf(key, sizeof(key), "Current_AC_Phase_%hhu", x);
                        if ((current = cJSON_GetObjectItemCaseSensitive(id, key)) == NULL) {
                            rc = FRONIUS_ERROR;
                            goto exit;
                        }
                        snprintf(key, sizeof(key), "Voltage_AC_Phase_%hhu", x);
                        if ((voltage = cJSON_GetObjectItemCaseSensitive(id, key)) == NULL) {
                            rc = FRONIUS_ERROR;
                            goto exit;
                        }
                        snprintf(key, sizeof(key), "PowerReal_P_Phase_%hhu", x);
                        if ((real = cJSON_GetObjectItemCaseSensitive(id, key)) == NULL) {
                            rc = FRONIUS_ERROR;
                            goto exit;
                        }
                        snprintf(key, sizeof(key), "PowerApparent_S_Phase_%hhu", x);
                        if ((apparent = cJSON_GetObjectItemCaseSensitive(id, key)) == NULL) {
                            rc = FRONIUS_ERROR;
                            goto exit;
                        }
                        if ((values->energy_meter.power_real[x - 1] = (float)real->valuedouble) >= 0) {
                            values->energy_meter.current[x - 1] = fabs((float)current->valuedouble);
                        }
                        else {
                            values->energy_meter.current[x - 1] = -fabs((float)current->valuedouble);
                        }
                        values->energy_meter.voltage[x - 1] = (float)voltage->valuedouble;
                        values->energy_meter.power_apparent[x - 1] = (float)apparent->valuedouble;
                    }
                    if ((energy_real_cons = cJSON_GetObjectItemCaseSensitive(id, "EnergyReal_WAC_Sum_Consumed")) == NULL) {
                        rc = FRONIUS_ERROR;
                        goto exit;
                    }
                    if ((energy_real_prod = cJSON_GetObjectItemCaseSensitive(id, "EnergyReal_WAC_Sum_Produced")) == NULL) {
                        rc = FRONIUS_ERROR;
                        goto exit;
                    }
                    values->energy_meter.energy_real_cons_wh = energy_real_cons->valueint;
                    values->energy_meter.energy_real_prod_wh = energy_real_prod->valueint;
                    if ((freq = cJSON_GetObjectItemCaseSensitive(id, "Frequency_Phase_Average")) == NULL) {
                        rc = FRONIUS_ERROR;
                        goto exit;
                    }
                    values->energy_meter.grid_freq_hz = (float)freq->valuedouble;
                    rc = FRONIUS_OK;
                    goto exit;
                }
            }
        }
    }

    rc = FRONIUS_ERROR;
    exit:
    if (json) {
        cJSON_free(json);
    }
    FREE_MEM(response);
    return rc;
}


static uint8_t get_powerflow_realtime_data(pv_values_t *values) {
    uint8_t rc;
    long status_code;
    char *response = NULL;
    cJSON *json = NULL;

    if (http_get("http://localhost:6000/solar_api/v1/GetPowerFlowRealtimeData.fcgi", &response, &status_code) != FRONIUS_OK) {
        rc = FRONIUS_ERROR;
        goto exit;
    }
    if (status_code != 200) {
        rc = FRONIUS_ERROR;
        goto exit;
    }
    if ((json = cJSON_Parse(response))) {
        cJSON *body;
        cJSON *data;
        cJSON *invertes;
        cJSON *id;
        cJSON *site;
        cJSON *p_load;
        cJSON *p_akku;;
        cJSON *soc;
        if ((body = cJSON_GetObjectItemCaseSensitive(json, "Body")) != NULL) {
            if ((data = cJSON_GetObjectItemCaseSensitive(body, "Data")) != NULL) {
                if ((invertes = cJSON_GetObjectItemCaseSensitive(data, "Inverters")) != NULL) {
                    if ((id = cJSON_GetObjectItemCaseSensitive(invertes, "1")) != NULL) {
                        if ((soc = cJSON_GetObjectItemCaseSensitive(id, "SOC")) == NULL) {
                            rc = FRONIUS_ERROR;
                            goto exit;
                        }
                        values->battery.battery_soc = (float)soc->valuedouble;
                    }
                }
                if ((site = cJSON_GetObjectItemCaseSensitive(data, "Site")) != NULL) {
                    if ((p_akku = cJSON_GetObjectItemCaseSensitive(site, "P_Akku")) == NULL) {
                        rc = FRONIUS_ERROR;
                        goto exit;
                    }
                    if ((p_load = cJSON_GetObjectItemCaseSensitive(site, "P_Load")) == NULL) {
                        rc = FRONIUS_ERROR;
                        goto exit;
                    }
                    values->battery.battery_power_w = p_akku->valueint;
                    values->energy_meter.p_load_w = p_load->valueint;
                }
                rc = FRONIUS_OK;
                goto exit;
            }
        }
    }

    rc = FRONIUS_ERROR;
    exit:
    if (json) {
        cJSON_free(json);
    }
    FREE_MEM(response);
    return rc;
}


uint8_t fronius_get_values(pv_values_t *values) {
    uint8_t rc;

    if (get_ivnerter_realtime_data(values) != FRONIUS_OK) {
        LOGE(TAG, "Error during getting inverter realtime values");
        rc = FRONIUS_ERROR;
        goto exit;
    }
    if (get_meter_realtime_data(values) != FRONIUS_OK) {
        LOGE(TAG, "Error during getting meter realtime values");
        rc = FRONIUS_ERROR;
        goto exit;
    }
    if (get_powerflow_realtime_data(values) != FRONIUS_OK) {
        LOGE(TAG, "Error during getting powerflow realtime values");
        rc = FRONIUS_ERROR;
        goto exit;
    }

    rc = FRONIUS_OK;
    exit:
    return rc;
}

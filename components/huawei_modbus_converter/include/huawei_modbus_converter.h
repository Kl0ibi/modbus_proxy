#include <stdint.h>
#include <stdbool.h>

typedef struct __attribute__((packed, aligned(2))){
    struct __attribute__((packed, aligned(2))){
        uint32_t pv_dc_w;
        int32_t inv_ac_w;
        uint32_t daily_pv_energy_wh;
        uint32_t total_pv_energy_wh;
    } inverter;
    struct __attribute__((packed, aligned(2))){
        float current[3];
        float voltage[3];
        float power_real[3];
        float power_apparent[3];
        uint64_t energy_apparent_cons_vah;
        uint64_t energy_real_cons_wh;
        uint64_t energy_apparent_prod_vah;
        uint64_t energy_real_prod_wh;
        int32_t p_grid_w;
        int32_t p_load_w;
        float grid_freq_hz;
    } energy_meter;
    struct __attribute__((packed, aligned(2))){
        int32_t battery_power_w;
        float battery_soc;
        uint8_t battery_working_mode;
    } battery;
} huawei_values_t;


typedef struct __attribute__((packed, aligned(2))) {
    struct __attribute__((packed, aligned(2))) {
        char model[32];
        char unique_id[32];
        uint32_t max_power_ac_w;
    } inverter;
    struct __attribute__((packed, aligned(2))) {
        char model[32];
    } energy_meter;
    struct __attribute__((packed, aligned(2))) {
        char model[32];
        char unique_id[32];
    } battery;
} huawei_info_t;



void huawei_get_info(huawei_info_t *info);
void huawei_get_values(huawei_values_t *values);

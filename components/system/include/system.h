#ifndef SYSTEM_H 
#define SYSTEM_H

#include <stdint.h>


#define FREE_MEM(x) if (x) {free(x); (x) = 0;}


#define MAX_UINTN(N) ((1U << (N)) - 1)
#define MAX_INTN(N) ((1 << ((N) - 1)) - 1)
#define MIN_INTN(N) (-(1 << ((N) - 1)))


typedef struct __attribute__((packed, aligned(2))){
    struct __attribute__((packed, aligned(2))){
        int32_t pv_dc_w;
        float pv1_voltage;
        float pv1_current;
        int32_t pv1_power;
        float pv2_voltage;
        float pv2_current;
        int32_t pv2_power;
        int32_t inv_ac_w;
        uint32_t daily_pv_energy_wh;
        uint32_t daily_inv_energy_wh;
        uint32_t total_pv_energy_wh;
        uint32_t total_inv_energy_wh;
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
        uint32_t daily_battery_charge_wh;
        uint32_t daily_battery_discharge_wh;
        uint32_t total_battery_charge_wh;
        uint32_t total_battery_discharge_wh;
    } battery;
} pv_values_t;


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
} pv_info_t;


void memcpy_reverse(void *d, void *s, unsigned char size);
void trim_leading_whitespace(char *str);
uint64_t get_time_in_milliseconds();


#endif

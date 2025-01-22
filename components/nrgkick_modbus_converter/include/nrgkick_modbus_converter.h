#include <stdint.h>
#include <stdbool.h>

typedef struct __attribute__((packed, aligned(2))) {
    float current[3];
    float connector_temp[3];
    uint32_t charged_energy;
    float charging_power;
    float housing_temp;
    uint8_t cp_status;
    uint8_t switched_relais;
    uint8_t warning_code;
    uint8_t error_code;
    float max_current_to_ev;
    float user_set_current;
} nrgkick_values_t;


void nrgkick_get_values(nrgkick_values_t *values);

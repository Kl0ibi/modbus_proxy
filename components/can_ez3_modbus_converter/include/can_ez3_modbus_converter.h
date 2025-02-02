#include <stdint.h>
#include <stdbool.h>


typedef struct __attribute__((packed, aligned(2))) {
    float boiler_temp;
    float sl_temp;
    uint16_t sl_power;
    uint16_t sl_status; 
} can_ez3_values_t;


void can_ez3_get_values(can_ez3_values_t *values);

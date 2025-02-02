#include <stdint.h>
#include <stdbool.h>


typedef enum {
    HUAWEI = 0,
    NRGKICK = 1,
    CAN_EZ3 = 2,
} poll_type_t;


void modbus_tcp_poll_get_client_data_str(poll_type_t type, uint16_t address, uint16_t length, bool reversed, char **data, uint16_t *len);
uint8_t *modbus_tcp_poll_get_client_data_raw(poll_type_t type, uint16_t address, uint16_t length, uint16_t *out_length);
void modbus_tcp_poll_get_client_data(poll_type_t type, uint16_t address, uint16_t length, bool big_endian, uint8_t *data);
void modbus_tcp_poll_stop_client(poll_type_t type);
void modbus_tcp_poll_stop_all();
bool modbus_tcp_poll_start_client(poll_type_t type, const char *host, uint16_t port);

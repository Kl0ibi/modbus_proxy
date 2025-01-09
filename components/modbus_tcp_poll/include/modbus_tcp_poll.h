#include <stdint.h>
#include <stdbool.h>

bool modbus_tcp_poll_start(const char *host, uint16_t port);
void modbus_tcp_poll_stop();
bool modbus_tcp_poll_running();
uint8_t *modbus_tcp_get_poll_data_raw(uint16_t address, uint16_t length, uint16_t *out_length);
void modbus_tcp_get_poll_data(uint16_t address, uint16_t length, bool big_endian, uint8_t *data);
void modbus_tcp_get_poll_str(uint16_t address, uint16_t length, bool reversed, char **data, uint16_t *len);

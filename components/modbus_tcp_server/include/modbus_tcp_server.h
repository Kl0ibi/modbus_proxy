#include <stdbool.h>
#include <stdint.h>


bool modbus_tcp_server_is_running();
void modbus_tcp_server_stop();
void modbus_tcp_server_start(uint16_t port);

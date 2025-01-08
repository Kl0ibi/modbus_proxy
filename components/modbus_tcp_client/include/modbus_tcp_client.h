#include <stdbool.h>
#include <stdint.h>


#define MODBUS_OK (1)
#define MODBUS_ERR (-1)

typedef enum {
	MB_READ_DO = 0x01, /*!< Read of digital output (discreet) */
	MB_READ_DI = 0x02, /*!< Read of digital input (discreet) */
	MB_READ_AO = 0x03, /*!< Read of analog output (16 Bit) */
	MB_READ_AI = 0x04, /*!< Read of analog input (16 Bit) */
} modbus_function_code_read;

typedef enum {
	MB_WRITE_DO __attribute__((unused)) = 0x05,				/*!< Write of digital output (discreet) */
	MB_WRITE_AO __attribute__((unused)) = 0x06,				/*!< Write of analog output (16 Bit) */
	MB_WRITE_MULTIPLE_DO __attribute__((unused)) = 0x0F,	/*!< Write of multiple digital outputs (discreet) */
	MB_WRITE_MULTIPLE_AO __attribute__((unused)) = 0x10,	/*!< Write of multiple analog outputs (16 Bit) */
} modbus_function_code_write;


int32_t modbus_tcp_client_read_str(int32_t sock, uint8_t unit_id, uint16_t start_reg, uint8_t num_regs, bool reversed, char **data, uint8_t *len, modbus_function_code_read function_code);
int32_t modbus_tcp_client_read(int32_t sock, uint8_t unit_id, uint16_t start_reg, uint8_t num_regs, bool big_endian, uint8_t *data, modbus_function_code_read function_code);
int32_t modbus_tcp_client_read_raw(int32_t sock, uint8_t unit_id, uint16_t start_reg, uint8_t num_regs, uint8_t *data, modbus_function_code_read function_code);
int32_t modbus_tcp_client_connect(const char *host, const uint16_t *port);
int32_t modbus_tcp_client_disconnect(int32_t sock);

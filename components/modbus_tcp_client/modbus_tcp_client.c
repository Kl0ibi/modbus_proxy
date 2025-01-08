#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "modbus_tcp_client.h"
#include "logging.h"


#define MB_DEFAULT_PORT (502)               // Default modbus port
#define MB_MIN_RESPONSE_MSG_LENGTH (9)      // Minimum modbus response message length
#define MB_RESPONSE_FUNCTION_INDEX (7)      // Modbus response message function-byte index
#define MB_RESPONSE_DATA_BYTE_INDEX (8)     // Modbus response message data-byte index
#define MB_REGISTER_SIZE_BYTE (2)           // Size of one modbus register
#define MB_RECV_BUFFER_SIZE (256)           // Max size of the response message buffer
#define MB_RECV_TIMEOUT_S (3)               // Max allowed timeout for connecting to server or receiving data


static const char *TAG = "modbus_tcp_client";


// region Type definitions
typedef struct {
	uint16_t transaction_id;	/*!< Transaction ID */
	uint16_t message_size;		/*!< Message size */
	uint8_t unit_id;			/*!< Address of slave */
	uint8_t function_code;		/*!< Function code (operation or error) */
	uint16_t start_reg;			/*!< Address of first register */
	uint16_t num_regs;			/*!< Number of registers */
} modbus_request_t;
// endregion


static void memcpy_reverse(void *d, void *s, unsigned char size) {
	unsigned char *pd = (unsigned char *) d;
	unsigned char *ps = (unsigned char *) s;

	ps += size;
	while (size--) {
		--ps;
		*pd++ = *ps;
	}
}


/**
 * @brief Build a modbus request.
 *
 * @details https://ipc2u.de/artikel/wissenswertes/detaillierte-beschreibung-des-modbus-tcp-protokolls-mit-befehlsbeispielen/
 *
 * @return
 *      -
 * */
static void build_modbus_request(uint8_t *req_buf, modbus_request_t *request) {
	req_buf[0] = request->transaction_id >> 0x08;      // Transaction ID - 1
	req_buf[1] = request->transaction_id & 0xff;       // Transaction ID - 2
	req_buf[2] = 0x00;                                 // Protocol ID - 1 (always 00)
	req_buf[3] = 0x00;                                 // Protocol ID - 2 (always 00)
	req_buf[4] = request->message_size >> 0x08;        // Message size - 1
	req_buf[5] = request->message_size & 0xff;         // Message size - 2
	req_buf[6] = request->unit_id;                     // Unit-ID of slave device
	req_buf[7] = request->function_code;               // Function code
	req_buf[8] = request->start_reg >> 0x08;           // Address of the first register - 1
	req_buf[9] = request->start_reg & 0xff;            // Address of the first register - 2
	req_buf[10] = request->num_regs >> 0x08;           // Address of the last register - 1
	req_buf[11] = request->num_regs & 0xff;            // Address of the last register - 2
}


static int32_t send_register_request(int32_t sock, uint8_t unit_id, uint16_t start_reg, uint8_t num_regs, modbus_function_code_read function_code) {
	modbus_request_t mb_req_raw;
	uint8_t mb_req_bytes[12];

	// Create modbus request struct, which is used for building the modbus request
	mb_req_raw.message_size = sizeof(mb_req_bytes) - 6; // Excluding transaction_id, protocol_id and message_size bytes
	mb_req_raw.unit_id = unit_id;
	mb_req_raw.function_code = function_code;
	mb_req_raw.start_reg = start_reg;
	mb_req_raw.num_regs = num_regs;
	mb_req_raw.transaction_id = 0x00;
	build_modbus_request(mb_req_bytes, &mb_req_raw); // Build the request to send in modbus format
	if (send(sock, mb_req_bytes, sizeof(mb_req_bytes), 0) < 0) { // Send the previous build request
		LOGE(TAG, "Error sending request!");
		return MODBUS_ERR;
	}

	return MODBUS_OK;
}


int32_t get_register_values(int32_t sock, uint8_t unit_id, uint16_t start_reg, uint8_t num_regs, uint8_t *rec_buf, modbus_function_code_read function_code) {
	int32_t resp_len;

	if (send_register_request(sock, unit_id, start_reg, num_regs, function_code) < 1) {
		return MODBUS_ERR;
	}
	resp_len = recv(sock, rec_buf, MB_RECV_BUFFER_SIZE, 0);
	if (resp_len < MB_MIN_RESPONSE_MSG_LENGTH) {
		if (resp_len == 0) {
			LOGE(TAG, "Unexpected close of socket connection at remote end!");
			return MODBUS_ERR;
    }
		else {
			LOGW(TAG, "response was too short - %ld chars", resp_len);
			return MODBUS_ERR;
		}
	}
	else if (rec_buf[MB_RESPONSE_FUNCTION_INDEX] & 0x80) { // Check operation index of response for error code
		// Read data byte for error code
		LOGD(TAG, "MODBUS exception response! Type=%d", rec_buf[MB_RESPONSE_DATA_BYTE_INDEX]);
		return MODBUS_ERR;
	}
	else if (resp_len != (MB_MIN_RESPONSE_MSG_LENGTH + 2 * num_regs)) { // Validate response size
		LOGD(TAG, "Incorrect response size is %ld expected %d", resp_len, (9 + 2 * num_regs));
		return MODBUS_ERR;
	}

	return MODBUS_OK;
}


int32_t modbus_tcp_client_read_str(int32_t sock, uint8_t unit_id, uint16_t start_reg, uint8_t num_regs, bool reversed, char **data, uint8_t *len, modbus_function_code_read function_code) {
	uint8_t rec_buf[MB_RECV_BUFFER_SIZE] = {0};

	*len = 0;
	if (get_register_values(sock, unit_id, start_reg,num_regs, rec_buf, function_code) != MODBUS_OK) {
		return MODBUS_ERR;
	}
#ifdef CONFIG_MODBUSTCP_PRINT_RAW_BYTES
	printf("raw string bytes: \n");
	uint8_t buffer_size = (((num_regs * MB_REGISTER_SIZE_BYTE) + MB_MIN_RESPONSE_MSG_LENGTH) > MB_RECV_BUFFER_SIZE) ? MB_RECV_BUFFER_SIZE - 1: ((num_regs * MB_REGISTER_SIZE_BYTE) + MB_MIN_RESPONSE_MSG_LENGTH - 1);
	for (uint8_t i = 0; i <= buffer_size; i++) {
		printf("0x%.2x ", rec_buf[i]);
	}
	printf("\n");
#endif
	if (reversed) {
		for (uint8_t i = (num_regs * MB_REGISTER_SIZE_BYTE) + MB_MIN_RESPONSE_MSG_LENGTH; i >= MB_MIN_RESPONSE_MSG_LENGTH; i--) {
			if (rec_buf[i] > 0x7F) {
				LOGW(TAG, "Invalid string received! Unknown char at index %hhu", *len);
				break;
			}
			else if (rec_buf[i] == '\0') {
				continue;
			}
			*len += 1;
		}
		*data = malloc(*len + 1); // TODO: maybe not allocate size 0 + 1 just set data on NULL;
		memcpy_reverse(*data, &rec_buf[MB_MIN_RESPONSE_MSG_LENGTH], *len);
		(*data)[*len] = '\0';
	}
	else {
		for (uint8_t i = 0; i < num_regs * MB_REGISTER_SIZE_BYTE; i++) {
			if (rec_buf[MB_MIN_RESPONSE_MSG_LENGTH + i] > 0x7F) {
				LOGW(TAG, "Invalid string received! Unknown char at index %hhu", *len);
				break;
			}
			else if (rec_buf[MB_MIN_RESPONSE_MSG_LENGTH + i] == '\0') {
				break;
			}
			*len += 1;
		}
		*data = malloc(*len + 1);
		memcpy(*data, &rec_buf[MB_MIN_RESPONSE_MSG_LENGTH], *len);
		(*data)[*len] = '\0';
	}
	LOGD(TAG, "Received modbus response data: %s", *data);

	return MODBUS_OK;
}


int32_t modbus_tcp_client_read(int32_t sock, uint8_t unit_id, uint16_t start_reg, uint8_t num_regs, bool big_endian, uint8_t *data, modbus_function_code_read function_code) {
	uint8_t rec_buf[MB_RECV_BUFFER_SIZE] = {0};

	if (get_register_values(sock, unit_id, start_reg, num_regs, rec_buf, function_code) != MODBUS_OK) {
		return MODBUS_ERR;
	}
#ifdef CONFIG_MODBUSTCP_PRINT_RAW_BYTES
	printf("raw: \n");
	uint8_t buffer_size = (((num_regs * MB_REGISTER_SIZE_BYTE) + MB_MIN_RESPONSE_MSG_LENGTH) > MB_RECV_BUFFER_SIZE) ? MB_RECV_BUFFER_SIZE - 1: ((num_regs * MB_REGISTER_SIZE_BYTE) + MB_MIN_RESPONSE_MSG_LENGTH - 1);
	for (uint8_t i = 0; i <= buffer_size; i++) {
		printf("0x%.2x ", rec_buf[i]);
	}
	printf("\n");
#endif
	if (big_endian) {
		memcpy_reverse(data, &rec_buf[MB_MIN_RESPONSE_MSG_LENGTH], num_regs * MB_REGISTER_SIZE_BYTE);
	}
	else {
		for (uint8_t i = 0; i < num_regs; i++) {
			data[i * MB_REGISTER_SIZE_BYTE] = rec_buf[MB_MIN_RESPONSE_MSG_LENGTH + i * MB_REGISTER_SIZE_BYTE + 1];
			data[i * MB_REGISTER_SIZE_BYTE + 1] = rec_buf[MB_MIN_RESPONSE_MSG_LENGTH + i * MB_REGISTER_SIZE_BYTE];
		}
	}

	return MODBUS_OK;
}


int32_t modbus_tcp_client_read_raw(int32_t sock, uint8_t unit_id, uint16_t start_reg, uint8_t num_regs, uint8_t *data, modbus_function_code_read function_code) {
	uint8_t rec_buf[MB_RECV_BUFFER_SIZE] = {0};

	if (get_register_values(sock, unit_id, start_reg,num_regs, rec_buf, function_code) != MODBUS_OK) {
		return MODBUS_ERR;
	}
#ifdef CONFIG_MODBUSTCP_PRINT_RAW_BYTES
	printf("raw: \n");
	uint8_t buffer_size = (((num_regs * MB_REGISTER_SIZE_BYTE) + MB_MIN_RESPONSE_MSG_LENGTH) > MB_RECV_BUFFER_SIZE) ? MB_RECV_BUFFER_SIZE - 1: ((num_regs * MB_REGISTER_SIZE_BYTE) + MB_MIN_RESPONSE_MSG_LENGTH - 1);
	for (uint8_t i = 0; i <= buffer_size; i++) {
		printf("0x%.2x ", rec_buf[i]);
	}
	printf("\n");
#endif

    for (uint8_t i = 0; i < num_regs; i++) {
        data[i * MB_REGISTER_SIZE_BYTE] = rec_buf[MB_MIN_RESPONSE_MSG_LENGTH + i * MB_REGISTER_SIZE_BYTE];
        data[i * MB_REGISTER_SIZE_BYTE + 1] = rec_buf[MB_MIN_RESPONSE_MSG_LENGTH + i * MB_REGISTER_SIZE_BYTE + 1];
    }

	return MODBUS_OK;
}


int32_t modbus_tcp_client_connect(const char *host, const uint16_t *port) {
	int32_t sock;
	int32_t addr_family;
	int32_t ip_protocol;
	uint16_t modbus_port = MB_DEFAULT_PORT;
	struct sockaddr_in dest_addr = {0};

	modbus_port = (port != NULL && *port != 0) ? *port : MB_DEFAULT_PORT;
	// Configure socket
	addr_family = AF_INET;
	ip_protocol = IPPROTO_TCP;
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(modbus_port); /* ASA standard port */
	dest_addr.sin_addr.s_addr = inet_addr(host);
	// Create socket
	LOGI(TAG, "New modbus TCP client socket: [%s]:[%u]", host, modbus_port);
	if ((sock = socket(addr_family, SOCK_STREAM, ip_protocol)) < 0) {
		LOGE(TAG, "Unable to create socket! Error=%d");
		return MODBUS_ERR;
	}
	// set receive Timeout
	struct timeval tv;
	tv.tv_sec = MB_RECV_TIMEOUT_S;
	tv.tv_usec = 0;
	LOGV(TAG, "Set send and receive timeout to %lld", tv.tv_sec);
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		LOGE(TAG, "Error setting receive timeout to %lld seconds.", tv.tv_sec);
		shutdown(sock, 0);
		close(sock);
		return MODBUS_ERR;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
		LOGE(TAG, "Error setting send timeout to %lld seconds.", tv.tv_sec);
		shutdown(sock, 0);
		close(sock);
		return MODBUS_ERR;
	}
	// Connect to server
	if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in)) != 0) {
		LOGW(TAG, "Socket unable to connect to [%s]:[%u]!", host, modbus_port);
		shutdown(sock, 0);
		close(sock);
		return MODBUS_ERR;
	}

	return sock;
}


int32_t modbus_tcp_client_disconnect(int32_t sock) {
	LOGD(TAG, "Closing socket connection! Socket=%li", sock);
	if (sock != -1) {
		shutdown(sock, 0);
		close(sock);
		return MODBUS_OK;
	}
	return MODBUS_ERR;
}

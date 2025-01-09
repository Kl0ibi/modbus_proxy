#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "modbus_tcp_client.h"
#include "logging.h"


#define TAG "modbus_tcp_poll"

#define DELAY_BETWEEN_REGISTERS_US (20000)
#define CONNECTION_DELAY_S (1)
#define DELAY_BETWEEN_POLLS_S (5 - CONNECTION_DELAY_S)
#define CONNECTION_RETRY_DELAY_S (5)


typedef struct {
    uint16_t address;
    uint16_t length;
    uint8_t function_code;
    uint8_t num_failed_requests;
} poll_register_t;


poll_register_t poll_registers[] = {
    {30000, 25, 0x03, 0},
    {30073, 4, 0x03, 0},
    {32080, 36, 0x03, 0},
    {37000, 62, 0x03, 0},
    {37100, 38, 0x03, 0},
    {37700, 67, 0x03, 0},
    {47000, 1, 0x03, 0},
    {47086, 4, 0x03, 0},

//{30000, 15, 0x03, 0},
//{30015, 10, 0x03, 0},
//{30073, 2, 0x03,0},
//{30075, 2, 0x03, 0},
//{32080, 2, 0x03, 0},
//{32114, 2, 0x03, 0},
//{37000, 1, 0x03, 0},
//{37052, 10, 0x03, 0},
//{37100, 1, 0x03, 0},
////{37101, 37, 0x03, 0},
//{37113, 2, 0x03, 0},
//{37125, 1, 0x03, 0},
//{37700, 10, 0x03, 0},
//{37741, 1, 0x03, 0},
//{37758, 2, 0x03, 0},
//{37760, 1, 0x03, 0},
//{37762, 1, 0x03, 0},
//{37765, 2, 0x03, 0},
//{47000, 1, 0x03, 0},
//{47086, 1, 0x03, 0},
//{47089, 1, 0x03, 0},

};


#define POLL_REGISTER_COUNT (sizeof(poll_registers) / sizeof(poll_register_t))
#define MAX_NUM_FAILED_REQ (5)

typedef struct {
    uint8_t *data;
    uint16_t length;
    uint16_t address;
} poll_data_t;


poll_data_t poll_data[POLL_REGISTER_COUNT];
pthread_mutex_t poll_data_mutex = PTHREAD_MUTEX_INITIALIZER;


static pthread_t polling_thread_id;
static bool polling_thread_running = false;
static pthread_mutex_t polling_control_mutex = PTHREAD_MUTEX_INITIALIZER;


static void memcpy_reverse(void *d, void *s, unsigned char size) {
	unsigned char *pd = (unsigned char *) d;
	unsigned char *ps = (unsigned char *) s;

	ps += size;
	while (size--) {
		--ps;
		*pd++ = *ps;
	}
}


uint8_t *modbus_tcp_get_poll_data_raw(uint16_t address, uint16_t length, uint16_t *out_length) {
    uint8_t *data = NULL;

    pthread_mutex_lock(&poll_data_mutex);
    for (size_t i = 0; i < POLL_REGISTER_COUNT; i++) {
        uint16_t start_address = poll_data[i].address;
        uint16_t end_address = start_address + poll_data[i].length / 2;
        if (address >= start_address && (address + length) <= end_address) {
            uint16_t offset = (address - start_address) * 2;
            data = malloc(length * 2);
            if (data) {
                memcpy(data, poll_data[i].data + offset, length * 2);
                *out_length = length * 2;
            }
            break;
        }
    }

    pthread_mutex_unlock(&poll_data_mutex);
    return data;
}


void modbus_tcp_get_poll_data(uint16_t address, uint16_t length, bool big_endian, uint8_t *data) {
    size_t i;

    pthread_mutex_lock(&poll_data_mutex);
    for (i = 0; i < POLL_REGISTER_COUNT; i++) {
        uint16_t start_address = poll_data[i].address;
        uint16_t end_address = start_address + poll_data[i].length / 2;
        if (address >= start_address && (address + length) <= end_address) {
            uint16_t offset = (address - start_address) * 2;
            if (big_endian) {
                memcpy_reverse(data, poll_data[i].data + offset, length * 2);
            }
            else {
                for (uint8_t i = 0; i < length; i++) {
                    data[i * 2] = *(poll_data[i].data + (offset + i * 2 + 1));
                    data[i * 2 + 1] = *(poll_data[i].data + (offset + i * 2));
                }
            }
            break;
        }
    }
    if (i == POLL_REGISTER_COUNT) {
        LOGE(TAG, "No cached data found for address %u with length %u", address, length);
        memset(data, 0, length * 2);
    }
    pthread_mutex_unlock(&poll_data_mutex);
}


void modbus_tcp_get_poll_str(uint16_t address, uint16_t length, bool reversed, char **data, uint16_t *len) {
    size_t i;

    pthread_mutex_lock(&poll_data_mutex);
    for (i = 0; i < POLL_REGISTER_COUNT; i++) {
        uint16_t start_address = poll_data[i].address;
        uint16_t end_address = start_address + poll_data[i].length / 2;
        if (address >= start_address && (address + length) <= end_address) {
            uint16_t offset = (address - start_address) * 2;

            if (reversed) {
                for (uint8_t x = (offset + length * 2); x >= 0; x--) {
                    if (poll_data[i].data[x] > 0x7F) {
                        LOGW(TAG, "Invalid string received! Unknown char at index %hhu", *len);
                        break;
                    }
                    else if (poll_data[i].data[x] == '\0') {
                        continue;
                    }
                    *len += 1;
                }
                *data = malloc(*len + 1); // TODO: maybe not allocate size 0 + 1 just set data on NULL;
                memcpy_reverse(*data, poll_data[i].data + offset, *len);
                (*data)[*len] = '\0';
            }
            else {
                for (uint8_t x = offset; x < offset + length * 2; x++) {
                    if (poll_data[i].data[x] > 0x7F) {
                        LOGW(TAG, "Invalid string received! Unknown char at index %hhu", *len);
                        break;
                    }
                    else if (poll_data[i].data[x] == '\0') {
                        break;
                    }
                    *len += 1;
                }
                *data = malloc(*len + 1);
                memcpy(*data, poll_data[i].data + offset, *len);
                (*data)[*len] = '\0';
            }
            break;
        }
    }
    pthread_mutex_unlock(&poll_data_mutex);
}



void modbus_tcp_save_poll_data(uint16_t address, uint8_t *data, uint16_t length) {
    pthread_mutex_lock(&poll_data_mutex);

    for (size_t i = 0; i < POLL_REGISTER_COUNT; i++) {
        if (poll_data[i].address == address) {
            if (poll_data[i].data) {
                free(poll_data[i].data);
            }
            poll_data[i].data = malloc(length);
            if (poll_data[i].data) {
                memcpy(poll_data[i].data, data, length);
                poll_data[i].length = length;
                //printf("add save: %hu\n", address);
                //for (uint8_t i = 0; i < length; i++) {
                //    printf("%2X", data[i]);
                //}
                //printf("\n");
            }
            break;
        }
    }
    pthread_mutex_unlock(&poll_data_mutex);
}


void *polling_thread(void *arg) {
    char *host = ((char **)arg)[0];
    uint16_t port = *(uint16_t *)((char **)arg)[1];
    free(arg);

    while (polling_thread_running) {
        int32_t sock = -1;
        while (polling_thread_running && sock < 0) {
            sock = modbus_tcp_client_connect(host, &port);
            if (sock < 0) {
                LOGE(TAG, "Failed to connect to Modbus client, retrying...");
                for (size_t i = 0; i < POLL_REGISTER_COUNT; i++) {
                    if (poll_registers[i].num_failed_requests < MAX_NUM_FAILED_REQ - 1) {
                        poll_registers[i].num_failed_requests++;
                    LOGW(TAG, "Failed to poll address %u, let value cached (%u of %u retries)", poll_registers[i].address, poll_registers[i].num_failed_requests, MAX_NUM_FAILED_REQ);
                    }
                    else if (poll_registers[i].num_failed_requests > MAX_NUM_FAILED_REQ - 1) {
                        LOGW(TAG, "Failed to poll address %u, reached max retries: %u", poll_registers[i].address, MAX_NUM_FAILED_REQ);
                    }
                    else {
                    LOGW(TAG, "Failed to poll address %u, set to 0 (%u of %u retries)", poll_registers[i].address, poll_registers[i].num_failed_requests, MAX_NUM_FAILED_REQ);
                        poll_registers[i].num_failed_requests++;
                        uint8_t *buf = calloc(poll_registers[i].length * 2 + 9, sizeof(uint8_t));
                        if (!buf) {
                            LOGE(TAG, "Failed to allocate memory for poll data");
                            continue;
                        }
                        modbus_tcp_save_poll_data(poll_registers[i].address, buf, poll_registers[i].length * 2);
                        free(buf);
                    }
                }
                sleep(CONNECTION_RETRY_DELAY_S);
            }
        }
        if (!polling_thread_running) {
            if (sock >= 0) modbus_tcp_client_disconnect(sock);
            break;
        }
        sleep(CONNECTION_DELAY_S); // connection delay needed for huwaei dongle
        for (size_t i = 0; i < POLL_REGISTER_COUNT; i++) {
            poll_register_t *reg = &poll_registers[i];
            uint8_t *buf = calloc(reg->length * 2 + 9, sizeof(uint8_t)); // 2 bytes per register
            if (!buf) {
                LOGE(TAG, "Failed to allocate memory for poll data");
                continue;
            }
            LOGI(TAG, "Polling address %u with quantity: %u", reg->address, reg->length);
            if (modbus_tcp_client_read_raw(sock, 1, reg->address, reg->length, buf, reg->function_code) == MODBUS_OK) {
                reg->num_failed_requests = 0;
                modbus_tcp_save_poll_data(reg->address, buf, reg->length * 2);
            }
            else {
                if (reg->num_failed_requests < MAX_NUM_FAILED_REQ - 1) {
                    reg->num_failed_requests++;
                    LOGW(TAG, "Failed to poll address %u, let value cached (%u of %u retries)", reg->address, reg->num_failed_requests, MAX_NUM_FAILED_REQ);
                }
                else if (poll_registers[i].num_failed_requests > MAX_NUM_FAILED_REQ - 1) {
                    LOGW(TAG, "Failed to poll address %u, retries reached %u,", poll_registers[i].address, MAX_NUM_FAILED_REQ);
                }
                else {
                    reg->num_failed_requests++;
                    LOGW(TAG, "Failed to poll address %u, set value to (%u of %u retries)", reg->address, reg->num_failed_requests, MAX_NUM_FAILED_REQ);
                    modbus_tcp_save_poll_data(reg->address, buf, reg->length * 2);
                }
            }
            free(buf);
            usleep(DELAY_BETWEEN_REGISTERS_US);
        }
        modbus_tcp_client_disconnect(sock);
        if (polling_thread_running) {
            sleep(DELAY_BETWEEN_POLLS_S);
        }
    }
    pthread_exit(NULL);
}


bool modus_tcp_poll_running() {
    bool status;

    pthread_mutex_lock(&polling_control_mutex);
    status = polling_thread_running;
    pthread_mutex_unlock(&polling_control_mutex);

    return status;
}


void modbus_tcp_poll_stop() {
    LOGI(TAG, "Stopping Modbus Poll ...");
    pthread_mutex_lock(&polling_control_mutex);
    if (polling_thread_running) {
        polling_thread_running = false;
        pthread_join(polling_thread_id, NULL);
    }
    pthread_mutex_unlock(&polling_control_mutex);
    LOGI(TAG, "Modbus Poll stopped.");
}


bool modbus_tcp_poll_start(const char *host, uint16_t port) {
    pthread_mutex_lock(&polling_control_mutex);
    if (polling_thread_running) {
        pthread_mutex_unlock(&polling_control_mutex);
        return false;
    }
    polling_thread_running = true;
    for (size_t i = 0; i < POLL_REGISTER_COUNT; i++) {
        poll_data[i].address = poll_registers[i].address;
        poll_data[i].data = NULL;
        poll_data[i].length = 0;
    }
    char **args = malloc(2 * sizeof(void *));
    if (!args) {
        LOGE(TAG, "Failed to allocate memory for arguments");
        polling_thread_running = false;
        pthread_mutex_unlock(&polling_control_mutex);
        return false;
    }
    args[0] = strdup(host);
    args[1] = malloc(sizeof(uint16_t));
    if (!args[0] || !args[1]) {
        LOGE(TAG, "Failed to allocate memory for arguments");
        free(args[0]);
        free(args);
        polling_thread_running = false;
        pthread_mutex_unlock(&polling_control_mutex);
        return false;
    }
    memcpy(args[1], &port, sizeof(uint16_t));
    if (pthread_create(&polling_thread_id, NULL, polling_thread, args) != 0) {
        LOGE(TAG, "Failed to create polling thread");
        free(args[0]);
        free(args[1]);
        free(args);
        polling_thread_running = false;
        pthread_mutex_unlock(&polling_control_mutex);
        return false;
    }
    pthread_mutex_unlock(&polling_control_mutex);
    return true;
}

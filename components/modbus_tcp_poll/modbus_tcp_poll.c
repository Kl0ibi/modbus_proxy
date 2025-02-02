#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "system.h"
#include "modbus_tcp_client.h"
#include "solar_logger.h"
#include "logging.h"
#include "modbus_tcp_poll.h"

#define TAG "modbus_tcp_poll"

#define DELAY_BETWEEN_REGISTERS_US (20000)
#define DELAY_BETWEEN_POLLS_S (5)
#define CONNECTION_RETRY_DELAY_S (5)
#define MAX_NUM_FAILED_REQ (5)
#define MAX_CLIENTS 3

typedef struct {
    uint16_t address;
    uint16_t length;
    uint8_t function_code;
    uint8_t num_failed_requests;
} poll_register_t;

typedef struct {
    uint8_t *data;
    uint16_t length;
    uint16_t address;
} poll_data_t;

typedef struct {
    char *host;
    uint16_t port;
    uint8_t connection_delay_s;
    pthread_t thread_id;
    bool running;
    size_t num_registers;
    poll_register_t *registers;
    poll_data_t *poll_data;
    pthread_mutex_t control_mutex;
    pthread_mutex_t data_mutex;
    poll_type_t type;
    uint64_t start_time;
} polling_client_t;


poll_register_t huawei_registers[] = {
    {30000, 25, 0x03, 0},
    {30073, 4, 0x03, 0},
    {32016, 100, 0x03, 0},
    {37000, 62, 0x03, 0},
    {37100, 38, 0x03, 0},
    {37700, 88, 0x03, 0},
    {47000, 1, 0x03, 0},
    {47086, 4, 0x03, 0},
};


poll_register_t nrgkick_registers[] = {
    {194, 69, 0x03, 0},
    {1, 1, 0x04, 0},
};


poll_register_t can_ez3_registers[] = {
    {1, 4, 0x04, 0},
};


static polling_client_t *polling_clients[MAX_CLIENTS];
static size_t num_clients = 0;


static void modbus_tcp_save_poll_data(polling_client_t *client, uint16_t address, uint8_t *data, uint16_t length) {
    pthread_mutex_lock(&client->data_mutex);
    for (size_t i = 0; i < client->num_registers; i++) {
        if (client->registers[i].address == address) {
            if (client->poll_data[i].data) {
                FREE_MEM(client->poll_data[i].data);
            }
            client->poll_data[i].data = malloc(length);
            if (client->poll_data[i].data) {
                memcpy(client->poll_data[i].data, data, length);
                client->poll_data[i].length = length;
            }
            /*printf("type: %u, adress: %u length: %u data:\n", client->type, address, length);
            for (uint8_t x = 0; x < length; x++) {
                printf("%02X", client->poll_data[i].data[x]);
            }
            printf("\n");*/
            break;
        }
    }
    pthread_mutex_unlock(&client->data_mutex);
}


static uint8_t *modbus_tcp_get_poll_data_raw(polling_client_t *client, uint16_t address, uint16_t length, uint16_t *out_length) {
    uint8_t *data = NULL;

    pthread_mutex_lock(&client->data_mutex);
    for (size_t i = 0; i < client->num_registers; i++) {
        uint16_t start_address = client->poll_data[i].address;
        uint16_t end_address = start_address + client->poll_data[i].length;
        if (address >= start_address && (address + length) <= end_address) {
            if (!client->poll_data[i].data) {
                i = client->num_registers;
                break;
            }
            uint16_t offset = (address - start_address) * 2;
            data = malloc(length * 2);
            if (data) {
                memcpy(data, client->poll_data[i].data + offset, length * 2);
                *out_length = length * 2;
            }
            break;
        }
    }

    pthread_mutex_unlock(&client->data_mutex);
    return data;
}


static void modbus_tcp_get_poll_data(polling_client_t *client, uint16_t address, uint16_t length, bool big_endian, uint8_t *data) {
    size_t i;

    pthread_mutex_lock(&client->data_mutex);
    for (i = 0; i < client->num_registers; i++) {
        uint16_t start_address = client->registers[i].address;
        uint16_t end_address = start_address + client->registers[i].length;
        if (address >= start_address && (address + length) <= end_address) {
            if (!client->poll_data[i].data) {
                i = client->num_registers;
                break;
            }
            uint16_t offset = (address - start_address) * 2;
            if (big_endian) {
                memcpy_reverse(data, client->poll_data[i].data + offset, length * 2);
            }
            else {
                for (uint16_t x = 0; x < length; x++) {
                    data[x * 2] = *(client->poll_data[i].data + offset + x * 2 + 1);
                    data[x * 2 + 1] = *(client->poll_data[i].data + offset + x * 2);
                }
            }
            break;
        }
    }
    if (i == client->num_registers) {
        LOGE(TAG, "No cached data found for address %u with length %u", address, length);
        memset(data, 0, length * 2);
    }
    pthread_mutex_unlock(&client->data_mutex);
}


static void modbus_tcp_get_poll_str(polling_client_t *client, uint16_t address, uint16_t length, bool reversed, char **data, uint16_t *len) {
    size_t i;

    pthread_mutex_lock(&client->data_mutex);
    for (i = 0; i < client->num_registers; i++) {
        uint16_t start_address = client->registers[i].address;
        uint16_t end_address = start_address + client->registers[i].length;
        if (address >= start_address && (address + length) <= end_address) {
            if (!client->poll_data[i].data) {
                i = client->num_registers;
                break;
            }
            uint16_t offset = (address - start_address) * 2;
            if (reversed) {
                for (uint8_t x = (offset + length * 2); x >= 0; x--) {
                    if (client->poll_data[i].data[x] > 0x7F) {
                        LOGW(TAG, "Invalid string received! Unknown char at index %hhu", *len);
                        break;
                    } else if (client->poll_data[i].data[x] == '\0') {
                        continue;
                    }
                    *len += 1;
                }
                *data = malloc(*len + 1);
                memcpy_reverse(*data, client->poll_data[i].data + offset, *len);
                (*data)[*len] = '\0';
            }
            else {
                for (uint8_t x = offset; x < offset + length * 2; x++) {
                    if (client->poll_data[i].data[x] > 0x7F) {
                        LOGW(TAG, "Invalid string received! Unknown char at index %hhu", *len);
                        break;
                    } else if (client->poll_data[i].data[x] == '\0') {
                        break;
                    }
                    *len += 1;
                }
                *data = malloc(*len + 1);
                memcpy(*data, client->poll_data[i].data + offset, *len);
                (*data)[*len] = '\0';
            }
            break;
        }
    }
    if (i == client->num_registers) {
        LOGE(TAG, "No cached data found for address %u with length %u", address, length);
        *data = malloc(*len + 1);
        (*data)[*len] = '\0';
    }
    pthread_mutex_unlock(&client->data_mutex);
}


void *polling_thread(void *arg) {
    polling_client_t *client = (polling_client_t *)arg;
    int32_t sock = -1;

    while (client->running) {
        client->start_time = get_time_in_milliseconds();
        sock = modbus_tcp_client_connect(client->host, &client->port);
        if (sock < 0) {
            LOGE(TAG, "Failed to connect to Modbus client, retrying...");
            sleep(CONNECTION_RETRY_DELAY_S);
            continue;
        }
        sleep(client->connection_delay_s);
        for (size_t i = 0; i < client->num_registers; i++) {
            poll_register_t *reg = &client->registers[i];
            uint8_t *buf = calloc(reg->length * 2 + 9, sizeof(uint8_t));
            if (!buf) {
                LOGE(TAG, "Failed to allocate memory for poll data");
                continue;
            }
            LOGI(TAG, "Polling address %u with quantity: %u", reg->address, reg->length);
            if (modbus_tcp_client_read_raw(sock, 1, reg->address, reg->length, buf, reg->function_code) == MODBUS_OK) {
                reg->num_failed_requests = 0;
                modbus_tcp_save_poll_data(client, reg->address, buf, reg->length * 2);
            }
            else {
                if (reg->num_failed_requests < MAX_NUM_FAILED_REQ - 1) {
                    reg->num_failed_requests++;
                    LOGW(TAG, "Failed to poll address %u, let value cached (%u of %u retries)", reg->address, reg->num_failed_requests, MAX_NUM_FAILED_REQ);
                }
                else {
                    reg->num_failed_requests++;
                    LOGW(TAG, "Failed to poll address %u, retries reached %u", reg->address, MAX_NUM_FAILED_REQ);
                    memset(buf, 0, reg->length * 2);
                    modbus_tcp_save_poll_data(client, reg->address, buf, reg->length * 2);
                }
            }
            FREE_MEM(buf);
            usleep(DELAY_BETWEEN_REGISTERS_US);
        }
        modbus_tcp_client_disconnect(sock);

        if (client->type == HUAWEI) { // new huawei values are the trigger for db logging
            huawei_values_t huawei_data;
            nrgkick_values_t nrgkick_data;
            can_ez3_values_t can_ez3_data;
            huawei_get_values(&huawei_data);
            nrgkick_get_values(&nrgkick_data);
            can_ez3_get_values(&can_ez3_data);
            solar_logger_post_data(&huawei_data, &nrgkick_data, &can_ez3_data, true);
        }
        uint64_t diff_time = get_time_in_milliseconds() - client->start_time;
        if (diff_time < ((DELAY_BETWEEN_POLLS_S - client->connection_delay_s) * 1000)) {
            usleep((((DELAY_BETWEEN_POLLS_S - client->connection_delay_s) * 1000) - diff_time) * 1000);
        }
    }
    pthread_exit(NULL);
}


static void modbus_tcp_poll_stop(polling_client_t *client) {
    if (!client) return;
    LOGI(TAG, "Stopping Modbus Poll for client %s...", client->host);
    client->running = false;
    pthread_join(client->thread_id, NULL);
    FREE_MEM(client->host);
    for (size_t i = 0; i < client->num_registers; i++) {
        FREE_MEM(client->poll_data[i].data);
    }
    FREE_MEM(client->poll_data);
    pthread_mutex_destroy(&client->data_mutex);
    pthread_mutex_destroy(&client->control_mutex);
    FREE_MEM(client);
    LOGI(TAG, "Modbus poll client stopped");
}


static bool modbus_tcp_poll_start(poll_type_t type, const char *host, uint16_t port, uint8_t connection_delay_s, poll_register_t *registers, size_t num_registers) {
    if (num_clients >= MAX_CLIENTS) {
        LOGE(TAG, "Max number of clients reached");
        return false;
    }
    polling_client_t *client = malloc(sizeof(polling_client_t));
    if (!client) {
        LOGE(TAG, "Failed to allocate memory for new polling client");
        return false;
    }
    client->type = type;
    client->host = strdup(host);
    client->port = port;
    client->running = true;
    client->num_registers = num_registers;
    client->registers = registers;
    client->connection_delay_s = connection_delay_s;
    client->poll_data = malloc(sizeof(poll_data_t) * num_registers);
    pthread_mutex_init(&client->data_mutex, NULL);
    pthread_mutex_init(&client->control_mutex, NULL);
    for (size_t i = 0; i < num_registers; i++) {
        client->poll_data[i].address = registers[i].address;
        client->poll_data[i].data = NULL;
        client->poll_data[i].length = 0;
    }
    pthread_mutex_lock(&client->control_mutex);
    if (pthread_create(&client->thread_id, NULL, polling_thread, client) != 0) {
        LOGE(TAG, "Failed to create polling thread");
        modbus_tcp_poll_stop(client);
        pthread_mutex_unlock(&client->control_mutex);
        return false;
    }
    polling_clients[num_clients] = client;
    num_clients++;
    pthread_mutex_unlock(&client->control_mutex);

    return true;
}


void modbus_tcp_poll_get_client_data_str(poll_type_t type, uint16_t address, uint16_t length, bool reversed, char **data, uint16_t *len) {
    for (size_t i = 0; i < num_clients; i++) {
        if (polling_clients[i] && polling_clients[i]->type == type) {
            modbus_tcp_get_poll_str(polling_clients[i], address, length, reversed, data, len);
            break;
        }
    }
}


uint8_t *modbus_tcp_poll_get_client_data_raw(poll_type_t type, uint16_t address, uint16_t length, uint16_t *out_length) {
    uint8_t *data = NULL;
    for (size_t i = 0; i < num_clients; i++) {
        if (polling_clients[i] && polling_clients[i]->type == type) {
            data = modbus_tcp_get_poll_data_raw(polling_clients[i], address, length, out_length);
            break;
        }
    }
    return data;
}


void modbus_tcp_poll_get_client_data(poll_type_t type, uint16_t address, uint16_t length, bool big_endian, uint8_t *data) {
    for (size_t i = 0; i < num_clients; i++) {
        if (polling_clients[i] && polling_clients[i]->type == type) {
            modbus_tcp_get_poll_data(polling_clients[i], address, length, big_endian, data);
            break;
        }
    }
}


void modbus_tcp_poll_stop_client(poll_type_t type) {
    for (size_t i = 0; i < num_clients; i++) {
        if (polling_clients[i] && polling_clients[i]->type == type) {
            modbus_tcp_poll_stop(polling_clients[i]);
            polling_clients[i] = NULL;
            num_clients--;
            break;
        }
    }
}


void modbus_tcp_poll_stop_all() {
    for (size_t i = 0; i < num_clients; i++) {
        if (polling_clients[i] != NULL) {
            modbus_tcp_poll_stop(polling_clients[i]);
            polling_clients[i] = NULL;
        }
    }
    num_clients = 0;
}


bool modbus_tcp_poll_start_client(poll_type_t type, const char *host, uint16_t port) {
    if (type == HUAWEI) {
        return modbus_tcp_poll_start(HUAWEI, host, port, 1, huawei_registers, sizeof(huawei_registers) / sizeof(poll_register_t));
    }
    else if (type == NRGKICK) {
        return modbus_tcp_poll_start(NRGKICK, host, port, 0, nrgkick_registers, sizeof(nrgkick_registers) / sizeof(poll_register_t));
    }
    else if (type == CAN_EZ3) {
        return modbus_tcp_poll_start(CAN_EZ3, host, port, 0, can_ez3_registers, sizeof(can_ez3_registers) / sizeof(poll_register_t));
    }
    else {
        LOGE(TAG, "Invalid poll type");
        return false;
    }
}

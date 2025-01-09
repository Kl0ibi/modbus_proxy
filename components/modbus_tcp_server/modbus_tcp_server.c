#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include "logging.h"
#include "modbus_tcp_poll.h"


#define BUFFER_SIZE 256
#define TIMEOUT 60


static const char *TAG = "modbus_tcp_server";


typedef struct {
    int client_fd;
    time_t last_activity;
} client_info_t;


static bool server_running = false;
static pthread_t server_thread;
static int server_fd = -1;


static void set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        LOGE(TAG, "fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        LOGE(TAG, "fcntl(F_SETFL)");
        exit(EXIT_FAILURE);
    }
}


static void handle_modbus_request(client_info_t *client) {
    uint8_t buffer[BUFFER_SIZE];
    uint16_t start_address;
    uint16_t quantity;
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
    uint8_t function_code;
    uint8_t *data = NULL;
    uint16_t len = 0;
    ssize_t bytes_read = read(client->client_fd, buffer, sizeof(buffer));

    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            LOGI(TAG, "Client disconnected (fd: %d)", client->client_fd);
        } else if (errno != EAGAIN) {
            LOGE(TAG, "Internal error in read function");
        }
        close(client->client_fd);
        free(client);
        pthread_exit(NULL);
    }
    LOGD(TAG, "Received %zd bytes from client (fd: %d)", bytes_read, client->client_fd);

    client->last_activity = time(NULL);
    if (bytes_read < 12) {
        LOGW(TAG, "Invalid Modbus request size.");
        return;
    }
    transaction_id = (buffer[0] << 8) | buffer[1];
    protocol_id = (buffer[2] << 8) | buffer[3];
    length = (buffer[4] << 8) | buffer[5];
    unit_id = buffer[6];
    function_code = buffer[7];

    if (protocol_id != 0 || function_code != 3) {
        LOGW(TAG, "Unsupported Modbus request. Protocol ID: %u, Function Code: %u", protocol_id, function_code);
        return;
    }
    start_address = (buffer[8] << 8) | buffer[9];
    quantity = (buffer[10] << 8) | buffer[11];
    LOGI(TAG, "Received request for address %u and quantity %u", start_address, quantity);
    data = modbus_tcp_get_poll_data_raw(start_address, quantity, &len);
    if (data) {
        uint8_t *response;
        if ((response = calloc(1, (9 + len))) == NULL) {
            LOGE(TAG, "Data length exceeds buffer size. Requested: %u", len);
            free(data);
            return;
        }
        response[0] = buffer[0]; // Transaction ID high byte
        response[1] = buffer[1]; // Transaction ID low byte
        response[2] = 0x00;      // Protocol ID high byte
        response[3] = 0x00;      // Protocol ID low byte
        response[4] = ((3 + len) >> 8) & 0xFF; // Length high byte
        response[5] = (3 + len) & 0xFF;        // Length low byte
        response[6] = unit_id;                 // Unit ID
        response[7] = function_code;           // Function code (Read Holding Registers)
        response[8] = len;                     // Byte count (number of data bytes)
        for (size_t i = 0; i < len; i++) {
            response[9 + i] = data[i];
        }
        free(data);
        ssize_t bytes_written = write(client->client_fd, response, 9 + len);
        if (bytes_written < 0) {
            LOGE(TAG, "Error writing response to client. Closing connection.");
        }
        else {
            LOGI(TAG, "Sent response to client (fd: %d), byte_count: %u", client->client_fd, len);
        }
        //printf("0x");
        //for (uint8_t i = 0; i < bytes_written; i++) {
        //    printf("%02X", response[i]);
        //}
        //printf("\n");
        free(response);
    }
    else {
        LOGW(TAG, "No data found for address %u and quantity %u", start_address, quantity);
        uint8_t error_response[9] = {
            buffer[0], buffer[1], 0x00, 0x00, 0x00, 0x03, unit_id, function_code | 0x80, 0x02
        };
        write(client->client_fd, error_response, sizeof(error_response));
    }
}


void *client_thread(void *arg) {
    client_info_t *client = (client_info_t *)arg;

    while (server_running) {
        if (time(NULL) - client->last_activity > TIMEOUT) {
            LOGI(TAG, "Client timed out (fd: %d)", client->client_fd);
            close(client->client_fd);
            free(client);
            pthread_exit(NULL);
        }
        handle_modbus_request(client);
        usleep(10000);
    }

    close(client->client_fd);
    free(client);
    pthread_exit(NULL);
}


void *modbus_tcp_server_thread(void *arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    uint16_t server_port = *(uint16_t *)arg;

    free(arg);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOGE(TAG, "Socket creation failed");
        server_running = false;
        return NULL;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOGE(TAG, "Bind failed");
        close(server_fd);
        server_running = false;
        return NULL;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        LOGE(TAG, "Listen failed");
        close(server_fd);
        server_running = false;
        return NULL;
    }

    LOGI(TAG, "Modbus TCP server listening on port %hu", server_port);

    while (server_running) {
        client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (!server_running) {
            break; 
        }
        if (client_fd < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue; // Retry on interrupt or non-blocking mode
            }
            LOGE(TAG, "Accept failed");
            continue;
        }

        LOGI(TAG, "Accepted new client (fd: %d)", client_fd);

        // Allocate memory for client info
        client_info_t *client = malloc(sizeof(client_info_t));
        if (!client) {
            LOGE(TAG, "Memory allocation failed");
            close(client_fd);
            continue;
        }

        client->client_fd = client_fd;
        client->last_activity = time(NULL);

        // Create a thread to handle the client
        pthread_t client_thread_id;
        if (pthread_create(&client_thread_id, NULL, client_thread, client) != 0) {
            LOGE(TAG, "Error creating client thread");
            close(client_fd);
            free(client);
        }

        pthread_detach(client_thread_id); // Detach the client thread
    }

    close(server_fd);
    server_fd = -1;
    return NULL;
}


bool modbus_tcp_server_is_running() {
    return server_running;
}

void modbus_tcp_server_stop() {
    LOGI(TAG, "Stopping Modbus TCP server...");
    server_running = false;
    if (server_fd >= 0) {
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
        server_fd = -1;
    }
    pthread_join(server_thread, NULL);
    LOGI(TAG, "Modbus TCP server stopped.");
}

void modbus_tcp_server_start(uint16_t port) {
    uint16_t *server_port;
    if (server_running) {
        LOGI(TAG, "Modbus TCP server is already running");
        return;
    }
    if ((server_port = malloc(sizeof(uint16_t))) == NULL) {
        LOGE(TAG, "Error allocating memory for server port");
        exit(EXIT_FAILURE);
    }
    server_running = true;
    memcpy(server_port, &port, sizeof(uint16_t));
    if (pthread_create(&server_thread, NULL, modbus_tcp_server_thread, server_port) != 0) {
        LOGE(TAG, "Error creating Modbus TCP server thread");
        free(server_port);
        exit(EXIT_FAILURE);
    }
}

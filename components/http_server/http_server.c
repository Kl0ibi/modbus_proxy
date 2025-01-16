#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include "system.h"
#include "modbus_tcp_poll.h"
#include "huawei_modbus_converter.h"
#include "logging.h"


#define TAG "http_server"


#define ROOT_URI "/"
#define VALUES_URI "/values"
#define INFO_URI "/info"


#define BUFFER_SIZE 1024


#define WEBSERVER_OK 0
#define WEBSERVER_GENERAL_ERROR 1



typedef enum {
    TYPE_URI_UNKNOWN = 0,
    TYPE_URI_ROOT = 1,
    TYPE_URI_VALUES = 2,
    TYPE_URI_INFO = 3,
} request_uri_type;


const char* status_code_200 = "200 OK";
const char* status_code_404 = "404 Not Found";
const char* status_code_500 = "500 Internal Server Error";

const char* content_type_json = "application/json";
const char* content_type_html = "text/html";


int server_sock = -1;
int running = false;
pthread_t server_thread;
uint16_t server_port = 80;


// NOTE: if function returns WEBSERVER_OK, buffer must be freed


static uint8_t build_payload(request_uri_type type, char **buffer, size_t *buffer_size) {
    char *body = NULL;
    size_t body_size = 0;
    const char *status_code;
    const char *content_type;

    if (type == TYPE_URI_VALUES) {
        huawei_values_t values;
        huawei_get_values(&values);
        body_size = asprintf(&body, 
            "{\"inverter\":{\"pv_dc_w\":%u,\"inv_ac_w\":%d,\"total_pv_energy_wh\":%u,\"daily_pv_energy_wh\":%u},"
            "\"energy_meter\":{\"p_grid_w\":%d,\"p_load_w\":%d,\"freq_hz\":%.3f,"
            "\"L1\":{\"current\":%.2f,\"voltage\":%.1f,\"power_real\":%.0f,\"power_apparent\":%.0f},"
            "\"L2\":{\"current\":%.2f,\"voltage\":%.1f,\"power_real\":%.0f,\"power_apparent\":%.0f},"
            "\"L3\":{\"current\":%.2f,\"voltage\":%.1f,\"power_real\":%.0f,\"power_apparent\":%.0f},"
            "\"energy_apparent_cons_vah\":%llu,\"energy_real_cons_wh\":%llu,"
            "\"energy_apparent_prod_vah\":%llu,\"energy_real_prod_wh\":%llu},"
            "\"battery\":{\"battery_power_w\":%d,\"battery_soc\":%.2f,"
            "\"battery_working_mode\":%u}}", 
            values.inverter.pv_dc_w, 
            values.inverter.inv_ac_w, 
            values.inverter.total_pv_energy_wh, values.inverter.daily_pv_energy_wh,
            values.energy_meter.p_grid_w, values.energy_meter.p_load_w, values.energy_meter.grid_freq_hz,
            values.energy_meter.current[0], values.energy_meter.voltage[0], 
            values.energy_meter.power_real[0], values.energy_meter.power_apparent[0],
            values.energy_meter.current[1], values.energy_meter.voltage[1], 
            values.energy_meter.power_real[1], values.energy_meter.power_apparent[1],
            values.energy_meter.current[2], values.energy_meter.voltage[2], 
            values.energy_meter.power_real[2], values.energy_meter.power_apparent[2],
            values.energy_meter.energy_apparent_cons_vah,
            values.energy_meter.energy_real_cons_wh,
            values.energy_meter.energy_apparent_prod_vah,
            values.energy_meter.energy_real_prod_wh,
            values.battery.battery_power_w,
            values.battery.battery_soc,
            values.battery.battery_working_mode
        );
        status_code = status_code_200;
        content_type = content_type_json;
    }
    else if (type == TYPE_URI_INFO) {
        huawei_info_t info;
        huawei_get_info(&info);
        body_size = asprintf(&body, 
            "{\"inverter\":{\"model\":\"%s\",\"unique_id\":\"%s\",\"max_power_ac_w\":%u},"
            "\"energy_meter\":{\"model\":\"%s\"},"
            "\"battery\":{\"model\":\"%s\",\"unique_id\":\"%s\"}}", 
            info.inverter.model, 
            info.inverter.unique_id, 
            info.inverter.max_power_ac_w,
            info.energy_meter.model,
            info.battery.model, 
            info.battery.unique_id
        );
        status_code = status_code_200;
        content_type = content_type_json;
    }
    else {
        if (!(body_size = asprintf(&body, "available uris:<br>/values<br>/info<br>"))) {
            return WEBSERVER_GENERAL_ERROR;
        }
        if (type == TYPE_URI_ROOT) {
            status_code = status_code_200;
        }
        else {
            status_code = status_code_404;
        }
        content_type = content_type_html;
    }
    *buffer_size = asprintf(buffer,
                            "HTTP/1.1 %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "\r\n"
            "%s",
             status_code, content_type, body_size, body);
    if (*buffer_size <= 0) {
        FREE_MEM(body);
        *buffer_size = 0;
        return WEBSERVER_GENERAL_ERROR;
    }

    FREE_MEM(body);
    return WEBSERVER_OK;
}


void* handle_request(void* arg) {
    int client_sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    char method[8];
    char url[128];
    request_uri_type uri_type;
    FREE_MEM(arg);

    bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        LOGE(TAG, "Failed to read from client");
        close(client_sock);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    if (sscanf(buffer, "%s %s", method, url) != 2) {
        LOGE(TAG, "Malformed HTTP request\n");
        close(client_sock);
        return NULL;
    }
    if (strcmp(method, "GET")) {
        LOGW(TAG,  "Unsupported HTTP method: %s\n", method);
        close(client_sock);
        return NULL;
    }
    if (!strcmp(url, ROOT_URI)) {
        uri_type = TYPE_URI_ROOT;
    }
    else if (!strcmp(url, VALUES_URI)) {
        uri_type = TYPE_URI_VALUES;
    }
    else if (!strcmp(url, INFO_URI)) {
        uri_type = TYPE_URI_INFO;
    }
    else {
        uri_type = TYPE_URI_UNKNOWN;
    }
    char *body = NULL;


    char *payload = NULL;
    size_t payload_len = 0;
    build_payload(uri_type, &payload, &payload_len);
    if (payload_len) {
        write(client_sock, payload, payload_len);
    }
    FREE_MEM(payload);
    close(client_sock);
    return NULL;
}

void* http_server_loop(void* arg) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    fd_set read_fds;
    struct timeval timeout;

    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(server_sock, &read_fds);

        timeout.tv_sec = 1;  // Timeout after 1 second
        timeout.tv_usec = 0;

        int activity = select(server_sock + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            if (errno != EINTR) {
                LOGE(TAG, "select error");
            }
        } else if (FD_ISSET(server_sock, &read_fds)) {
            int* client_sock = malloc(sizeof(int));
            if (!client_sock) {
                LOGE(TAG, "Failed to allocate memory for client socket");
                continue;
            }

            *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
            if (*client_sock < 0) {
                LOGE(TAG, "Failed to accept client connection");
                FREE_MEM(client_sock);
                continue;
            }

            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_request, client_sock) != 0) {
                LOGE(TAG, "Failed to create thread");
                close(*client_sock);
                FREE_MEM(client_sock);
            } else {
                pthread_detach(thread); // Detach thread to clean up resources
            }
        }
    }

    return NULL;
}

int http_server_start(uint16_t port) {
    struct sockaddr_in server_addr;
    server_port = port;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        LOGE(TAG, "Failed to create socket");
        return -1;
    }
    int optval = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        LOGE(TAG,"setsockopt(SO_REUSEADDR) failed");
        close(server_sock);
        return -1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOGE(TAG, "Failed to bind socket");
        close(server_sock);
        return -1;
    }
    if (listen(server_sock, 10) < 0) {
        LOGE(TAG, "Failed to listen on socket");
        close(server_sock);
        return -1;
    }
    running = true;
    if (pthread_create(&server_thread, NULL, http_server_loop, NULL) != 0) {
        LOGE(TAG, "Failed to create server thread");
        running = 0;
        close(server_sock);
        return -1;
    }
    LOGI(TAG, "HTTP server started on port %u", port);

    return 0;
}


void http_server_stop() {
    LOGI(TAG, "Stopping http server");
    running = false;
    if (server_sock >= 0) {
        close(server_sock);
        shutdown(server_sock, SHUT_RDWR);
        server_sock = -1;
    }
    if (server_thread) {
        pthread_join(server_thread, NULL);
    }
    LOGI(TAG, "Http server stopped.");
}

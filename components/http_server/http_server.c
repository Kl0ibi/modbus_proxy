#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>
#include "modbus_tcp_poll.h"
#include "logging.h"


#define TAG "http_server"

#define FREE_MEM(x) if (x) {free(x); (x) = 0;}

#define ROOT_URI "/"
#define VALUES_URI "/values"
#define INFO_URI "/info"


#define BUFFER_SIZE 1024


#define WEBSERVER_OK 0
#define WEBSERVER_GENERAL_ERROR 1


typedef struct __attribute__((packed, aligned(2))){
    int32_t power_real_l3;
    int32_t power_real_l2;
    int32_t power_real_l1;
    int32_t l3_l1_line_voltage;
    int32_t l2_l3_line_voltage;
    int32_t l1_l2_line_voltage;
    uint16_t meter_type;
    int32_t energy_reactive_cons;
    int32_t energy_real_cons;
    int32_t energy_real_prod;
    int16_t grid_freq;
    int16_t power_sf;
    int32_t total_reactive_power;
    int32_t total_real_power;
    int32_t current_l3;
    int32_t current_l2;
    int32_t current_l1;
    int32_t voltage_l3;
    int32_t voltage_l2;
    int32_t voltage_l1;
} huawei_em_request_t;


typedef struct __attribute__((packed, aligned(2))){
    struct __attribute__((packed, aligned(2))){
        uint32_t pv_dc_w;
        int32_t inv_ac_w;
        uint32_t daily_pv_energy_wh;
        uint32_t total_pv_energy_wh;
    } inverter;
    struct __attribute__((packed, aligned(2))){
        float current[3];
        float voltage[3];
        float power_real[3];
        float power_apparent[3];
        uint64_t energy_apparent_cons_vah;
        uint64_t energy_real_cons_wh;
        uint64_t energy_apparent_prod_vah;
        uint64_t energy_real_prod_wh;
        int32_t p_grid_w;
        int32_t p_load_w;
        float grid_freq_hz;
    } energy_meter;
    struct __attribute__((packed, aligned(2))){
        int32_t battery_power_w;
        float battery_soc;
        uint8_t battery_working_mode;
    } battery;
} huawei_values_t;


typedef struct __attribute__((packed, aligned(2))) {
    struct __attribute__((packed, aligned(2))) {
        char model[32];
        char unique_id[32];
        uint32_t max_power_ac_w;
    } inverter;
    struct __attribute__((packed, aligned(2))) {
        char model[32];
    } energy_meter;
    struct __attribute__((packed, aligned(2))) {
        char model[32];
        char unique_id[32];
    } battery;
} huawei_info_t;


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
static void get_modbus_values(huawei_values_t *values) {
    int32_t temp_pv_dc_w;
	uint16_t temp_bat_soc;
	uint16_t temp_bat_working_mode;
    huawei_em_request_t em;

    modbus_tcp_get_poll_data(32080, 2, true, (uint8_t *)&values->inverter.inv_ac_w);
    if (isnan((double)values->inverter.inv_ac_w)) {
        values->inverter.inv_ac_w = 0;
    }
    modbus_tcp_get_poll_data(32106, 2, true, (uint8_t *)&values->inverter.total_pv_energy_wh);
    if (isnan((double)values->inverter.total_pv_energy_wh)) {
        values->inverter.total_pv_energy_wh = 0;
    }
    else {
        values->inverter.total_pv_energy_wh *= 100;
    }
    modbus_tcp_get_poll_data(32114, 2, true, (uint8_t *)&values->inverter.daily_pv_energy_wh);
    if (isnan((double)values->inverter.daily_pv_energy_wh)) {
        values->inverter.daily_pv_energy_wh = 0;
    }
    else {
        values->inverter.daily_pv_energy_wh *= 10;
    }
    modbus_tcp_get_poll_data(37765, 2, true, (uint8_t *)&values->battery.battery_power_w);
    if (isnan((double)values->battery.battery_power_w)) {
        values->battery.battery_power_w = 0;
    }
    else {
        values->battery.battery_power_w = (-values->battery.battery_power_w); // huawei inverted
    }
    modbus_tcp_get_poll_data(37760, 1, true, (uint8_t *)&temp_bat_soc);
    if (isnan((double)temp_bat_soc)) {
        values->battery.battery_soc = 0;
    }
    else {
        values->battery.battery_soc = (float)(temp_bat_soc) / 10;
    }
    modbus_tcp_get_poll_data(47086, 1, true, (uint8_t *)&temp_bat_working_mode);
    if (isnan((double)temp_bat_working_mode)) {
        values->battery.battery_working_mode = 0;
    }
    temp_pv_dc_w = values->inverter.inv_ac_w - values->battery.battery_power_w;
    values->inverter.pv_dc_w = temp_pv_dc_w < 0 ? 0 : temp_pv_dc_w;
	values->energy_meter.p_load_w = values->inverter.inv_ac_w - values->energy_meter.p_grid_w; // TODO: here is grid already needed
    modbus_tcp_get_poll_data(37101, 37, true, (uint8_t *)&em);
    values->energy_meter.p_grid_w = (-em.total_real_power); // huawei is inverted
    values->energy_meter.grid_freq_hz = (float)(em.grid_freq) / 100;
    values->energy_meter.voltage[0] = (float)(em.voltage_l1) / 10;
    values->energy_meter.power_real[0] = -(float)(em.power_real_l1);
    values->energy_meter.current[0] = -(float)(em.current_l1) / 100;
    values->energy_meter.power_apparent[0] = -(float)((float)em.power_real_l1 / (float)em.power_sf);
    if (em.meter_type) {
        values->energy_meter.power_real[1] = -(float)em.power_real_l2;
        values->energy_meter.power_real[2] = -(float)em.power_real_l3;
        values->energy_meter.current[1] = -(float)(em.current_l2) / 100;
        values->energy_meter.current[2] = -(float)(em.current_l3) / 100;
        values->energy_meter.voltage[1] = (float)(em.voltage_l2) / 10;
        values->energy_meter.voltage[2] = (float)(em.voltage_l1) / 10;
        values->energy_meter.power_apparent[1] = -(float)((float)em.power_real_l2 / (float)em.power_sf);
        values->energy_meter.power_apparent[2] = -(float)((float)em.power_real_l3 / (float)em.power_sf);
    }
    else {
        for (uint8_t i = 1; i < 3; i++) {
            values->energy_meter.current[i] = 0;
            values->energy_meter.voltage[i] = 0;
            values->energy_meter.power_real[i] = 0;
            values->energy_meter.power_apparent[i] = 0;
        }
    }
    values->energy_meter.energy_real_cons_wh = em.energy_real_cons * 10;
    values->energy_meter.energy_real_prod_wh = em.energy_real_prod * 10;
    if (em.energy_reactive_cons > 0) {
        values->energy_meter.energy_apparent_cons_vah = (uint64_t)sqrt((double)(values->energy_meter.energy_real_cons_wh * values->energy_meter.energy_real_cons_wh) + (em.energy_reactive_cons * 10 * em.energy_reactive_cons * 10));
        values->energy_meter.energy_apparent_prod_vah = 0;
    }
    else {
        values->energy_meter.energy_apparent_prod_vah = (uint64_t)sqrt((double)(values->energy_meter.energy_real_cons_wh * values->energy_meter.energy_real_cons_wh) + (em.energy_reactive_cons * 10 * em.energy_reactive_cons * 10));
        values->energy_meter.energy_apparent_cons_vah = 0;
    }
}


static char *huawei_battery_product_mode_to_string(uint16_t product) {
	switch (product) {
		case 1:
			return "LG-RESU";
		case 2:
			return "LUNA2000";
		default:
			return "-";
	}
}


static char *huawei_em_type_to_string(uint16_t type) {
	switch (type) {
		case 0:
			return "Single Phase";
		case 1:
			return "Three Phase";
		default:
			return "-";
	}
}


static void get_modbus_info(huawei_info_t *info) {
	char *inv_unique_id = NULL;
	uint16_t inv_unique_id_len;
	char *inv_model = NULL;
	uint16_t inv_model_len;
	uint16_t bat_status;
    char *bat_unique_id;
    uint16_t bat_unique_id_len;
	uint16_t bat_product_model;
	uint16_t meter_status;
	uint16_t meter_type;

    modbus_tcp_get_poll_str(30000, 15, false, &inv_model, &inv_model_len);
    if (inv_model_len > 0 && inv_model[0] != '\0') {
        memcpy(info->inverter.model, inv_model, sizeof(info->inverter.model));
    }
    else {
        info->inverter.model[0] = '\0';
    }
    FREE_MEM(inv_model);
    modbus_tcp_get_poll_str(30015, 10, false, &inv_unique_id, &inv_unique_id_len);
    if(inv_unique_id_len > 0 && inv_unique_id[0] != '\0') {
        memcpy(info->inverter.unique_id, inv_unique_id, sizeof(info->inverter.unique_id));
    }
    else {
        info->inverter.unique_id[0] = '\0';
    }
    FREE_MEM(inv_unique_id);
    modbus_tcp_get_poll_data(30073, 2, true, (uint8_t *)&info->inverter.max_power_ac_w);
    modbus_tcp_get_poll_data(37125 , 1, true, (uint8_t *)&meter_type);
    memcpy(info->energy_meter.model, huawei_em_type_to_string(meter_type), sizeof(info->energy_meter.model));
    uint8_t model_len = 0;
    uint8_t unique_len = 0;
    modbus_tcp_get_poll_data( 37000, 1, true, (uint8_t *)&bat_status);
    if (bat_status != 0) {
        modbus_tcp_get_poll_data(47000, 1, true, (uint8_t *)&bat_product_model);
        model_len = snprintf(info->battery.model, sizeof(info->battery.model), "%s", huawei_battery_product_mode_to_string(bat_product_model));
        modbus_tcp_get_poll_str(37052, 10, false, &bat_unique_id, &bat_unique_id_len);
        if (bat_unique_id_len > 0 && bat_unique_id[0] != '\0') { // bat1
            memcpy(info->battery.unique_id, bat_unique_id, sizeof(info->battery.unique_id));
            unique_len = bat_unique_id_len;
        }
        FREE_MEM(bat_unique_id);
    }
    modbus_tcp_get_poll_data( 37741, 1, true, (uint8_t *)&bat_status);
    if (bat_status != 0) { // bat2
        modbus_tcp_get_poll_data(47089, 1, true, (uint8_t *)&bat_product_model);
        model_len = snprintf(info->battery.model + model_len, sizeof(info->battery.model), "%s%s", model_len ? " + ": "", huawei_battery_product_mode_to_string(bat_product_model));
        if (unique_len == 0) {
            modbus_tcp_get_poll_str(37700, 10, false, &bat_unique_id, &bat_unique_id_len);
            if (bat_unique_id_len > 0 && bat_unique_id[0] != '\0') {
                memcpy(info->battery.unique_id, bat_unique_id, sizeof(info->battery.unique_id));
                unique_len = bat_unique_id_len;
            }
        }
        FREE_MEM(bat_unique_id);
    }
    if (model_len == 0) {
        info->battery.model[0] = '\0';
    }
    if (unique_len == 0) {
        info->battery.unique_id[0] = '\0';
    }
}


static uint8_t build_payload(request_uri_type type, char **buffer, size_t *buffer_size) {
    char *body = NULL;
    size_t body_size = 0;
    const char *status_code;
    const char *content_type;

    if (type == TYPE_URI_VALUES) {
        huawei_values_t values;
        get_modbus_values(&values);
        body_size = asprintf(&body, 
            "{\"inverter\":{\"pv_dc_w\":%u,\"inv_ac_w\":%d,\"total_pv_energy_wh\":%u,\"daily_pv_energy_wh\":%u},"
            "\"energy_meter\":{\"p_grid_w\":%d,\"p_load_w\":%d,\"freq_hz\":%f,"
            "\"L1\":{\"current\":%f,\"voltage\":%f,\"power_real\":%f,\"power_apparent\":%f},"
            "\"L2\":{\"current\":%f,\"voltage\":%f,\"power_real\":%f,\"power_apparent\":%f},"
            "\"L3\":{\"current\":%f,\"voltage\":%f,\"power_real\":%f,\"power_apparent\":%f},"
            "\"energy_apparent_cons_vah\":%lu,\"energy_real_cons_wh\":%lu,"
            "\"energy_apparent_prod_vah\":%lu,\"energy_real_prod_wh\":%lu},"
            "\"battery\":{\"battery_power_w\":%d,\"battery_soc\":%f,"
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
        get_modbus_info(&info);
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

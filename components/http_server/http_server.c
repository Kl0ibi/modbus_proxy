#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <math.h>
#include "modbus_tcp_poll.h"


#define ROOT_URI "/"
#define VALUES_URI "/values"
#define INFO_URI "/info"


#define BUFFER_SIZE 1024

#define WEBSERVER_OK 0
#define WEBSERVER_GENERAL_ERROR 1



typedef struct {
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


typedef struct {
    struct {
        uint32_t pv_dc_w;
        int32_t inv_ac_w;
        uint32_t pv_energy_wh;
    } inverter;
    struct {
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
    } energy_meter;
    struct {
        int32_t battery_power_w;
        float battery_soc; // TODO: temp var is needed
        uint8_t battery_working_mode; // TODO: temp var is needed
    } battery;
} huawei_values_t;


typedef enum {
    TYPE_URI_UNKNOWN = 0,
    TYPE_URI_ROOT = 1,
    TYPE_URI_VALUES = 2,
    TYPE_URI_INFO = 3,
} request_uri_type;


//modbus_registers
//

const char* status_code_200 = "200 OK";
const char* status_code_404 = "404 Not Found";
const char* status_code_500 = "500 Internal Server Error";

const char* content_type_json = "application/json";
const char* content_type_html = "text/html";


int server_sock = -1;              // Server socket
int running = false;                   // Server running flag
pthread_t server_thread;           // Server thread
int server_port = 8888;            // Default port


// NOTE: if function returns WEBSERVER_OK, buffer must be freed

//static void calculate_and_convert_modbus_values(int32_t *pv_inv_w, uint32_t *pv_dc_w, uint32_t *pv_energy_wh, int32_t *p_akku_w, uint16_t *bat_soc, uint16_t *bat_working_mode, int32_t *p_grid_w) {
static void calculate_and_convert_modbus_values(huawei_values_t *values) {
    int32_t temp_pv_dc_w;
	uint16_t temp_bat_soc;
	uint16_t temp_bat_working_mode;
    huawei_em_request_t em;

    modbus_tcp_get_poll_data(32080, 2, true, (uint8_t *)&values->inverter.inv_ac_w);
    if (isnan((double)values->inverter.inv_ac_w)) {
        values->inverter.inv_ac_w = 0;
    }
    modbus_tcp_get_poll_data(32114, 2, true, (uint8_t *)&values->inverter.pv_energy_wh);
    if (isnan((double)values->inverter.pv_energy_wh)) {
        values->inverter.pv_energy_wh = 0;
    }
    else {
        values->inverter.pv_energy_wh /= 100; // gain 100
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
        values->battery.battery_soc = temp_bat_soc / 10;
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


static uint8_t build_payload(request_uri_type type, char **buffer, size_t buffer_size) {
    char *body = NULL;
    size_t body_size = 0;
    const char *status_code;
    const char *content_type;

    if (type == TYPE_URI_VALUES) {



        status_code = status_code_200;
        content_type = content_type_json;
    }
    else if (type == TYPE_URI_INFO) {

        status_code = status_code_200;
        content_type = content_type_json;
    }
    else {
        if (!(body_size = asprintf(&body, "available uris:\n/values\n/info\n"))) {
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
    buffer_size = asprintf(buffer,
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status_code, content_type, body_size, body);
    if (buffer_size < 0) {
        free(body);
        return WEBSERVER_GENERAL_ERROR;
    }

    free(body);
    return WEBSERVER_OK;
}


void* handle_request(void* arg) {
    int client_sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    char method[8];
    char url[128];
    request_uri_type uri_type;
    free(arg);

    bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        perror("Failed to read from client");
        close(client_sock);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    if (sscanf(buffer, "%s %s", method, url) != 2) {
        fprintf(stderr, "Malformed HTTP request\n");
        close(client_sock);
        return NULL;
    }
    if (strcmp(method, "GET")) {
        fprintf(stderr, "Unsupported HTTP method: %s\n", method);
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
        uri_type = TYPE_URI_VALUES;
    }
    else {
        uri_type = TYPE_URI_UNKNOWN;
    }
    char *body = NULL;


    huawei_values_t values;
    calculate_and_convert_modbus_values(&values);
    printf("bat_soc: %f\n", values.battery.battery_soc);
    const char* response_body = status_code_200;
    // Prepare the HTTP response
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             strlen(response_body), response_body);

    // Send the response to the client
    write(client_sock, response, strlen(response));

    // Close the connection
    close(client_sock);
    return NULL;
}

// Server thread function
void* server_loop(void* arg) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    printf("Server running on http://localhost:%d\n", server_port);

    while (running) {
        int* client_sock = malloc(sizeof(int));
        if (!client_sock) {
            perror("Failed to allocate memory for client socket");
            continue;
        }

        *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (*client_sock < 0) {
            perror("Failed to accept client connection");
            free(client_sock);
            continue;
        }

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_request, client_sock) != 0) {
            perror("Failed to create thread");
            close(*client_sock);
            free(client_sock);
        } else {
            pthread_detach(thread); // Detach thread to clean up resources
        }
    }

    return NULL;
}

// Function to start the server
int http_server_start(int port) {
    struct sockaddr_in server_addr;
    server_port = port;

    // Create a socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Failed to create socket");
        return -1;
    }

    // Bind the socket to the specified port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        close(server_sock);
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_sock, 10) < 0) {
        perror("Failed to listen on socket");
        close(server_sock);
        return -1;
    }

    running = true;
    if (pthread_create(&server_thread, NULL, server_loop, NULL) != 0) {
        perror("Failed to create server thread");
        running = 0;
        close(server_sock);
        return -1;
    }

    return 0;
}

// Function to stop the server
void http_server_stop() {
    running = false;

    // Close the server socket
    if (server_sock >= 0) {
        close(server_sock);
        server_sock = -1;
    }

    // Wait for the server thread to finish
    pthread_join(server_thread, NULL);

    printf("Server stopped.\n");
}

// Main function
/*int main(int argc, char* argv[]) {
    int port = 8888; // Default port

    // Parse the port number from the command-line arguments
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            return 1;
        }
    }

    // Start the server
    if (start_server(port) != 0) {
        fprintf(stderr, "Failed to start the server\n");
        return 1;
    }

    // Wait for a signal to stop the server (e.g., CTRL+C)
    printf("Press CTRL+C to stop the server...\n");
    signal(SIGINT, (void (*)(int))stop_server);
    pause();

    return 0;
}*/

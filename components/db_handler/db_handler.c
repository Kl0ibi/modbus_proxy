#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "system.h"
#include "logging.h"
#include "db_handler.h"


#define INFLUXDB_HOST "192.168.8.90"
#define INFLUXDB_PORT 8086
#define BUCKET "solarlogging"
#define ORG "kloibi"
#define TOKEN "okXO2ONiJFr0jJyCq-YkY52OXsg5bFjSDlg9UHj-KMUjtkrakaHmp1y9N20bf4z3VrDRAe0txNOYDiej9K2Eig=="

#define TAG "db_handler"


uint8_t db_handler_send_solar_values(char* formatted_values, size_t formatted_values_len) {
    uint8_t rc;
    int sock;
    struct sockaddr_in server;
    char *request = NULL;
    ssize_t request_len;
    char *response = NULL;
    ssize_t response_len;
    #define RESPONSE_BUF_SIZE 256

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        LOGE(TAG, "Socket creation failed");
        return DB_HANDLER_GENERAL_ERROR;
    }
    server.sin_addr.s_addr = inet_addr(INFLUXDB_HOST);
    server.sin_family = AF_INET;
    server.sin_port = htons(INFLUXDB_PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        LOGE(TAG, "Connection failed");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }
    LOGD(TAG, "Connected to InfluxDB server.");
    if ((request_len = asprintf(&request,
                 "POST /api/v2/write?bucket=%s&org=%s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Authorization: Token %s\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n"
                 "%s",
                 BUCKET, ORG, INFLUXDB_HOST, INFLUXDB_PORT, TOKEN, formatted_values_len, formatted_values)) <= 0) {

        LOGE(TAG, "Failed to allocate memory for request");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }
    if (send(sock, request, strlen(request), 0) < 0) {
        LOGE(TAG, "Failed to send request");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }
    FREE_MEM(request);

    response = malloc(RESPONSE_BUF_SIZE);
    if (!response) {
        LOGE(TAG, "Memory allocation failed");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }
    if ((response_len = recv(sock, response, RESPONSE_BUF_SIZE, 0)) <= 0) {
        LOGE(TAG, "Failed to receive response");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }
    else {
        if (response_len < RESPONSE_BUF_SIZE) {
            response[response_len] = '\0';
        }
        trim_leading_whitespace(response);
        LOGV(TAG, "Response from InfluxDB: %s", response);
        if (strncmp(response, "HTTP/1.1 204", 12) == 0) {
            LOGD(TAG, "Received status code 204");
        }
        else {
            LOGE(TAG, "Received unexpected status code");
            rc = DB_HANDLER_GENERAL_ERROR;
            goto exit;
        }
    }

    rc = DB_HANDLER_OK;
    exit:
    FREE_MEM(request);
    FREE_MEM(response);
    close(sock);
    return rc;
}

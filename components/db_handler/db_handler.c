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
#define TOKEN ""

#define TAG "db_handler"


static uint8_t query_influxdb(char *query, size_t *query_len) {
    int sock;
    struct sockaddr_in server;
    char *request = NULL;
    char *response = NULL;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return DB_HANDLER_GENERAL_ERROR;
    }
    server.sin_addr.s_addr = inet_addr(INFLUXDB_HOST);
    server.sin_family = AF_INET;
    server.sin_port = htons(INFLUXDB_PORT);
if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connection failed");
        free(query);
        return DB_HANDLER_GENERAL_ERROR;
    }
    printf("Connected to InfluxDB server.\n");

    if (asprintf(&request,
                 "POST /api/v2/query?org=%s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Authorization: Token %s\r\n"
                 "Content-Type: application/vnd.flux\r\n"
                 "Accept: application/csv\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n"
                 "%s",
                 ORG, INFLUXDB_HOST, INFLUXDB_PORT, TOKEN, *query_len, query) == -1) {
        perror("Failed to allocate memory for request");
        close(sock);
        return DB_HANDLER_GENERAL_ERROR;
    }
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("Failed to send request");
        free(request);
        close(sock);
        exit(1);
    }
    free(request);
    response = malloc(8192); // TODO: check if the db responds with a header that indicates the size of the response so it could be dynamic
    if (!response) {
        perror("Memory allocation failed");
        close(sock);
        exit(1);
    }
    ssize_t received = recv(sock, response, 8191, 0); // Leave room for null-terminator
    if (received < 0) {
        perror("Failed to receive response");
    } else {
        response[received] = '\0'; // Null-terminate the response
        printf("Response from InfluxDB:\n%s\n", response);
    }

    free(response);
    close(sock);

    return DB_HANDLER_OK; //TODO: compelte this code later
}


uint8_t db_handler_request_within_range(char *db_start, char *db_stop, uint32_t *ts_start, uint32_t *ts_stop) {
    uint8_t rc;
    char *query;
    size_t query_len;;
    char* start;
    char* stop;

    if (db_start && db_stop) {
        asprintf(&start, "\"%s\"", db_start);
        asprintf(&stop, "\"%s\"", db_stop);
    }
    else if (ts_start && ts_stop) {
        asprintf(&start, "%u", *ts_start);
        asprintf(&stop, "%u", *ts_stop);
    }
    else {
        return DB_HANDLER_GENERAL_ERROR;
    }
    if ((query_len = asprintf(&query,
                 "from(bucket: \"%s\")\n"
                 "  |> range(start: %s, stop: %s)\n"
                 "  |> filter(fn: (r) => r._measurement == \"solar_production\")\n"
                 "  |> pivot(rowKey: [\"_time\"], columnKey: [\"_field\"], valueColumn: \"_value\")\n"
                 "  |> keep(columns: [\"_time\", \"panel\", \"irradiance\", \"production\", \"temperature\"])\n"
                 "  |> map(fn: (r) => ({ r with _time: int(v: r._time / 1000000000) }))",
                 BUCKET, start, stop)) <= 0) {
        LOGE(TAG, "Failed to allocate memory for Flux query");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }


    rc = DB_HANDLER_OK;
    exit:
    FREE_MEM(start);
    FREE_MEM(stop);
    FREE_MEM(query);
    return rc;
}


uint8_t db_handler_send_solar_values(char* formatted_values, size_t formatted_values_len) {
    uint8_t rc;
    int sock;
    struct sockaddr_in server;
    char *request = NULL;
    size_t request_len;
    char *response = NULL;
    size_t response_len;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return DB_HANDLER_GENERAL_ERROR;
    }
    server.sin_addr.s_addr = inet_addr(INFLUXDB_HOST);
    server.sin_family = AF_INET;
    server.sin_port = htons(INFLUXDB_PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connection failed");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }
    printf("Connected to InfluxDB server.\n");

    if ((request_len = asprintf(&request,
                 "POST /api/v2/write?bucket=%s&org=%s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Authorization: Token %s\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n"
                 "%s",
                 BUCKET, ORG, INFLUXDB_HOST, INFLUXDB_PORT, TOKEN, formatted_values_len, formatted_values)) <= 0) {

        perror("Failed to allocate memory for request");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }
    printf("request: %s\n", request);
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("Failed to send request");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }
    FREE_MEM(request);

    response = malloc(4096); // Adjust size as needed
    if (!response) {
        perror("Memory allocation failed");
        rc = DB_HANDLER_GENERAL_ERROR;
        goto exit;
    }

    ssize_t received = recv(sock, response, 4095, 0); // Leave room for null-terminator
    if (received < 0) {
        perror("Failed to receive response");
    }
    else {
        response[received] = '\0'; // Null-terminate the response
        printf("Response from InfluxDB:\n%s\n", response);
    }

    rc = DB_HANDLER_OK;
    exit:
    FREE_MEM(request);
    FREE_MEM(response);
    close(sock);
    return rc;
}

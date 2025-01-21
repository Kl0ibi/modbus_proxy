#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include "logging.h"
#include "modbus_tcp_server.h"
#include "modbus_tcp_poll.h"
#include "http_server.h"
#include "solar_logger.h"


#define TAG "modbus_tcp_proxy"


static volatile bool running = true;


void handle_signal(int sig) {
    LOGD(TAG, "Received signal %d, shutting down...", sig);
    running = false;
}


void event_loop() {
    int timeout = 5000;
    fd_set read_fds;
    struct timeval tv;

    while (running) {
        FD_ZERO(&read_fds);
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        if (select(0, &read_fds, NULL, NULL, &tv) < 0) {
            break;
        }
    }
}


int main() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    logging_set_global_log_level(LOG_LEVEL_INFO);

    solar_logger_init();
    modbus_tcp_server_start(5502);
    modbus_tcp_poll_start_client(HUAWEI, "192.168.8.90", 5502);
    modbus_tcp_poll_start_client(NRGKICK, "192.168.8.95", 502);
    http_server_start(8000);

    event_loop();

    LOGI(TAG, "Stopping services...");
    modbus_tcp_server_stop();
    modbus_tcp_poll_stop_all();
    http_server_stop();
    solar_logger_deinit();
    LOGI(TAG, "Shutdown complete.");

    return 0;
}

typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE
} log_level_t;


void log_message(log_level_t level, const char* tag, const char* format, ...);
void logging_set_global_log_level(log_level_t level);


#define LOGE(tag, fmt, ...) log_message(LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) log_message(LOG_LEVEL_WARN, tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...) log_message(LOG_LEVEL_INFO, tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) log_message(LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOGV(tag, fmt, ...) log_message(LOG_LEVEL_VERBOSE, tag, fmt, ##__VA_ARGS__)

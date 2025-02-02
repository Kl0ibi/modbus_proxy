#include <time.h>
#include <sys/time.h>
#include "system.h"


void memcpy_reverse(void *d, void *s, unsigned char size) {
	unsigned char *pd = (unsigned char *) d;
	unsigned char *ps = (unsigned char *) s;

	ps += size;
	while (size--) {
		--ps;
		*pd++ = *ps;
	}
}


void trim_leading_whitespace(char *str) {
    char *start = str;
    
    while (*start == ' ' || *start == '\n' || *start == '\r' || *start == '\t') {
        start++;
    }
    if (start != str) {
        while (*start) {
            *str++ = *start++;
        }
        *str = '\0';
    }
}


uint64_t get_time_in_milliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

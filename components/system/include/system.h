#include <stdint.h>


#define FREE_MEM(x) if (x) {free(x); (x) = 0;}


#define MAX_UINTN(N) ((1U << (N)) - 1)
#define MAX_INTN(N) ((1 << ((N) - 1)) - 1)
#define MIN_INTN(N) (-(1 << ((N) - 1)))


void memcpy_reverse(void *d, void *s, unsigned char size);
void trim_leading_whitespace(char *str);
uint64_t get_time_in_milliseconds();

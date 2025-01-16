
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

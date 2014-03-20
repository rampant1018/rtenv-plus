#include "string.h"
#include "clib.h"

//this function helps to show int
void itoa(int n, char *dst, int base)
{
	char buf[33] = {0};
	char *p = &buf[32];

	if (n == 0)
		*--p = '0';
	else {
		unsigned int num = (base == 10 && num < 0) ? -n : n;

		for (; num; num/=base)
			*--p = "0123456789ABCDEF" [num % base];
		if (base == 10 && n < 0)
			*--p = '-';
	}

	strcpy(dst, p);
}

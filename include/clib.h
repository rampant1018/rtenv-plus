#ifndef CLIB_H
#define CLIB_H

#include <stdint.h>

void itoa(int n, char *dst, int base);
size_t fio_printf(int fd, const char *format, ...);

#endif

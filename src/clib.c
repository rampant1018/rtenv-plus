#include "string.h"
#include "clib.h"
#include "syscall.h"
#include <stdarg.h>

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

size_t fio_printf(int fd, const char *format, ...)
{
    int i, count = 0;
    int tmpint;
    char *tmpcharp;
    char tmpout[2] = {'0', '\0'};

    va_list(v1);
    va_start(v1, format);

    for(i = 0; format[i]; i++) {
        if(format[i] == '%') {
            switch(format[i + 1]) {
                case '%':
                    write(fd, "%", 2);
                    break;
                case 'd':
                case 'X':
                    tmpint = va_arg(v1, int);
                    itoa(tmpint, tmpcharp, (format[i + 1] == 'd' ? 10 : 16));
                    write(fd, tmpcharp, strlen(tmpcharp) + 1);
                    break;
                case 's':
                    tmpcharp = va_arg(v1, char *);
                    write(fd, tmpcharp, strlen(tmpcharp) + 1);
                    break;
            }
            i++;
        }
        else {
            tmpout[0] = format[i];
            write(fd, tmpout, 2);
        }
    }
    
    va_end(v1);
    return count;
}

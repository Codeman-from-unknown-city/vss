#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "utils.h"

void die(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	if (errno > 0)
		fprintf(stderr, ": %s", strerror(errno));
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}


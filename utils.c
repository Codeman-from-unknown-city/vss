#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "utils.h"

void die(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (errno == 0) {
		vsyslog(LOG_ERR, fmt, ap);
		goto exit;
	}
	int msglen = 0;
	char* msg = NULL;
	msglen = vsnprintf(msg, msglen, fmt, ap);
	va_end(ap);
	char* desc = strerror(errno);
	msglen += strlen(desc) + 3; // for ": " and '\0' (= 3)
	msg = malloc(msglen);
	va_start(ap, fmt);
	vsnprintf(msg, msglen, fmt, ap);
	va_end(ap);
	strcat(msg, ": ");
	strcat(msg, desc);
	syslog(LOG_ERR, msg);
exit:
	exit(EXIT_FAILURE);
}


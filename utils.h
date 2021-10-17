#pragma once

#include <string.h>

void die(const char* fmt, ...);

#define EXIT_IF_SYSCALL_FAILS(expr) if ((expr) == -1) die(#expr)

#define EXIT_WITH_MSG_IF_SYSCALL_FAILS(expr, errmsg, ...) \
	if ((expr) == -1) die(errmsg, ##__VA_ARGS__)

static inline void memclr(void* buf, size_t size)
{
	memset(buf, 0, size);
}



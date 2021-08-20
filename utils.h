#pragma once

#include <string.h>

void die(const char* fmt, ...);

static inline void memclr(void* buf, size_t size)
{
	memset(buf, 0, size);
}


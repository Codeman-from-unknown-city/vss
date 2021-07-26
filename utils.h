#pragma once

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define panic(fmt, ...)								\
{									     	\
	fprintf(stderr, "%s:%u: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
	exit(EXIT_FAILURE);						   	\
}									   	\

#define syserr(fn) panic("%s: %s", fn, strerror(errno))

static inline void memclr(void* buf, size_t size)
{
	memset(buf, 0, size);
}


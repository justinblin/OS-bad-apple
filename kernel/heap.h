#pragma once

#include <stdint.h>
#include <stddef.h>

extern void heapInit(void* start, size_t bytes);
extern "C" void* malloc(size_t size);
extern "C" void free(void* p);


#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stdint.h>

void *memcpy(void *dest, const void *src, uint32_t n);
void *memset(void *dest, int value, uint32_t n);

#endif
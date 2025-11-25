#ifndef __STRING_H_
#define __STRING_H_

#include <stddef.h>

void* memset(void* dest, int val, size_t count);
void* memcpy(void* dest, void* src, size_t count);
void* memmove(void* dest, const void* src, size_t count);
int memcmp(const void* b1, const void* b2, size_t count);
void* memchr(const void* b1, int val, size_t count);
void* memrchr(const void* b1, int val, size_t count);

#endif
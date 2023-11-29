#include "assert.h"
#include "stdio.h"


void ShowAssert(const char* file, uint32_t len)
{
    printf("assert,file=%s\r\nline=%d\r\n", file, len);
    while (1);
}



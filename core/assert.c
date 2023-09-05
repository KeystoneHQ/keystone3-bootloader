/**************************************************************************************************
 * Copyright (c) keyst.one. 2020-2025. All rights reserved.
 * Description: Assert handler.
 * Author: leon sun
 * Create: 2023-4-7
 ************************************************************************************************/


#include "assert.h"
#include "stdio.h"


void ShowAssert(const char* file, uint32_t len)
{
    printf("assert,file=%s\r\nline=%d\r\n", file, len);
    while (1);
}



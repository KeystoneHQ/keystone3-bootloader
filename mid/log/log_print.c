/**************************************************************************************************
 * Copyright (c) keyst.one. 2020-2025. All rights reserved.
 * Description: LOG打印接口.
 * Author: leon sun
 * Create: 2022-11-14
 ************************************************************************************************/

#include "log_print.h"
#include "stdio.h"


void PrintArray(const char *name, const uint8_t *data, uint16_t length)
{
    printf("%s,length=%d\r\n", name, length);
    for (uint32_t i = 0; i < length; i++) {
        if (i % 32 == 0 && i != 0) {
            printf("\r\n");
        }
        printf("%02X ", data[i]);
    }
    printf("\r\n");
}

void PrintU16Array(const char *name, const uint16_t *data, uint16_t length)
{
    printf("%s,length=%d\r\n", name, length);
    for (uint32_t i = 0; i < length; i++) {
        if (i % 16 == 0 && i != 0) {
            printf("\r\n");
        }
        printf("%5d ", data[i]);
    }
    printf("\r\n");
}


void PrintU32Array(const char *name, const uint32_t *data, uint16_t length)
{
    printf("%s,length=%d\r\n", name, length);
    for (uint32_t i = 0; i < length; i++) {
        if (i % 8 == 0 && i != 0) {
            printf("\r\n");
        }
        printf("%10d ", data[i]);
    }
    printf("\r\n");
}

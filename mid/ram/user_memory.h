/**************************************************************************************************
 * Copyright (c) keyst.one 2020-2025. All rights reserved.
 * Description: On-chip SRAM and external PSRAM heap memory management.
 * Author: leon sun
 * Create: 2022-11-8
 ************************************************************************************************/

#ifndef _USER_MEMORY_H
#define _USER_MEMORY_H

#include "stdint.h"
#include "stdbool.h"


void *pvPortMalloc(size_t xWantedSize);
void vPortFree(void * pv);


#endif

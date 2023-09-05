/**************************************************************************************************
 * Copyright (c) keyst.one 2020-2025. All rights reserved.
 * Description: system init. Always build this file for updating build time.
 * Author: leon sun
 * Create: 2022-11-8
 ************************************************************************************************/

#ifndef _DRV_SYS_H
#define _DRV_SYS_H

#include "stdint.h"
#include "stdbool.h"

void SystemClockInit(void);
void NvicInit(void);
void DelayMs(uint32_t ms);
void PrintSystemInfo(void);

#endif

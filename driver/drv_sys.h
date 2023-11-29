#ifndef _DRV_SYS_H
#define _DRV_SYS_H

#include "stdint.h"
#include "stdbool.h"

void SystemClockInit(void);
void NvicInit(void);
void DelayMs(uint32_t ms);
void PrintSystemInfo(void);

#endif

/**************************************************************************************************
 * Copyright (c) keyst.one 2020-2025. All rights reserved.
 * Description: uart 用户驱动.
 * Author: leon sun
 * Create: 2022-11-8
 ************************************************************************************************/

#ifndef _DRV_UART_H
#define _DRV_UART_H

#include "stdint.h"
#include "stdbool.h"


typedef void (*UartRcvByteCallbackFunc_t)(uint8_t byte);

void Uart0Init(UartRcvByteCallbackFunc_t func);

#endif

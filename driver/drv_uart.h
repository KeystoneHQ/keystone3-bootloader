#ifndef _DRV_UART_H
#define _DRV_UART_H

#include "stdint.h"
#include "stdbool.h"


typedef void (*UartRcvByteCallbackFunc_t)(uint8_t byte);

void Uart0Init(UartRcvByteCallbackFunc_t func);

#endif

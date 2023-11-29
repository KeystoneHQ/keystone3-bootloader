#ifndef _LOG_PRINT_H
#define _LOG_PRINT_H

#include "stdint.h"
#include "stdbool.h"

void PrintArray(const char *name, const uint8_t *data, uint16_t length);
void PrintU16Array(const char *name, const uint16_t *data, uint16_t length);
void PrintU32Array(const char *name, const uint32_t *data, uint16_t length);


#endif

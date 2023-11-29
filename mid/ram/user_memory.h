#ifndef _USER_MEMORY_H
#define _USER_MEMORY_H

#include "stdint.h"
#include "stdbool.h"


void *pvPortMalloc(size_t xWantedSize);
void vPortFree(void * pv);


#endif

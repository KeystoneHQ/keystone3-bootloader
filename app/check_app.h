/**************************************************************************************************
 * Copyright (c) keyst.one. 2020-2025. All rights reserved.
 * Description: Check app flash zone data.
 * Author: leon sun
 * Create: 2023-7-11
 ************************************************************************************************/


#ifndef _CHECK_APP_H
#define _CHECK_APP_H

#include "stdint.h"
#include "stdbool.h"
#include "err_code.h"

bool CheckApp(void);
bool CheckAppFactory(void);
void GetSoftwareVersion(uint32_t *major, uint32_t *minor, uint32_t *build);
int32_t GetSoftwareVersionFormData(uint32_t *major, uint32_t *minor, uint32_t *build, const uint8_t *data, uint32_t dataLen);

#endif

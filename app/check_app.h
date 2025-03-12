#ifndef _CHECK_APP_H
#define _CHECK_APP_H

#include "stdint.h"
#include "stdbool.h"
#include "err_code.h"

bool CheckAppExist(void);
bool CheckApp(void);
bool CheckAppFactory(void);
void GetSoftwareVersion(uint32_t *major, uint32_t *minor, uint32_t *build);
int32_t GetSoftwareVersionFromData(uint32_t *major, uint32_t *minor, uint32_t *build, const uint8_t *data, uint32_t dataLen);

#endif

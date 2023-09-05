/**************************************************************************************************
 * Copyright (c) Keystone. 2020-2025. All rights reserved.
 * Description: Check app flash zone data.
 * Author: leon sun
 * Create: 2023-7-24
 ************************************************************************************************/


#ifndef _RECOVERY_MODE_H
#define _RECOVERY_MODE_H

#include "stdint.h"
#include "stdbool.h"
#include "err_code.h"

bool OptionToRecoveryMode(void);
void RecoveryMode(void);
void RefreshFileStatus(void);

#endif

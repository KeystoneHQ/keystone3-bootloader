#ifndef _RECOVERY_MODE_H
#define _RECOVERY_MODE_H

#include "stdint.h"
#include "stdbool.h"
#include "err_code.h"

bool OptionToRecoveryMode(void);
void RecoveryMode(void);
void RefreshFileStatus(void);
void WipeDeviceCallbackFunc(void);
void EnterRecoveryMode(void);

#endif

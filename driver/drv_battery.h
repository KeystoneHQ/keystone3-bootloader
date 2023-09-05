/**************************************************************************************************
 * Copyright (c) keyst.one. 2020-2025. All rights reserved.
 * Description: battary driver.
 * Author: leon sun
 * Create: 2023-1-6
 ************************************************************************************************/


#ifndef _DRV_BATTARY_H
#define _DRV_BATTARY_H

#include "stdint.h"
#include "stdbool.h"
#include "err_code.h"

/// @brief Battery init, including ADC init.
/// @param
void BatteryInit(void);



/// @brief Get battery voltage.
/// @param
/// @return Battery voltage, in millivolts.
uint32_t GetBatteryMilliVolt(void);


#endif

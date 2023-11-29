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

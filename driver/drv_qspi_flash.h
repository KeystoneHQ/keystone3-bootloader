/**************************************************************************************************
 * Copyright (c) keyst.one. 2020-2025. All rights reserved.
 * Description: QSPI flash driver.
 * Author: leon sun
 * Create: 2023-1-19
 ************************************************************************************************/

#ifndef _DRV_QSPI_FLASH_H
#define _DRV_QSPI_FLASH_H

#include "stdint.h"
#include "stdbool.h"
#include "err_code.h"

/// @brief  QSPI flash init, get model.
/// @param
void QspiFlashInit(void);


/// @brief Get battery voltage.
/// @param addr Erase address.
void QspiFlashErase(uint32_t addr);


/// @brief
/// @param addr Write flash addr.
/// @param data
/// @param len
void QspiFlashWrite(uint32_t addr, const uint8_t *data, uint32_t len);

void QspiFlashEraseAndWrite(uint32_t addr, const uint8_t *data, uint32_t len);


#endif

#ifndef _FIRMWARE_UPDATE_H
#define _FIRMWARE_UPDATE_H

#include "stdint.h"
#include "stdbool.h"
#include "err_code.h"

#define APP_ADDR        (0x1001000 + 0x80000)

#define OTA_FILE_INFO_MARK_MAX_LEN          32
#define SIGNATURE_LEN                       128

#define SIGNATURE_ENABLE                    1
#define VERSION_CHECK_ENABLE                1
#define SD_CARD_KEYSTONE3_PATH              "0:keystone3.bin"
#define UPDATE_KEYSTONE3_PATH               "1:keystone3.bin"

//OTA file head info.m
typedef struct {
    char mark[OTA_FILE_INFO_MARK_MAX_LEN];
    uint32_t fileSize;
    uint32_t originalFileSize;
    uint8_t hash[32];
    uint8_t originalHash[32];
    uint32_t encode;
    uint32_t encodeUnit;
    uint32_t encrypt;
    char destPath[128];
    uint32_t originalBriefSize;
    uint32_t originalBriefCrc32;
    char signature[256];
    char originalSignature[256];
} OtaFileInfo_t;

/// @brief Update firmware storaged in SD card or USB mass storage device.
/// @param
void FirmwareUpdate(char *ilePath);
bool CalculateCheckSum(bool updateCheck, const uint8_t *originalHash);
Error_Code CopyBin2Flash(bool app2app);
void JumpToApp(void);
bool GetBootSecureCheckFlag(void);
bool GetRecoveryModeFlag(void);
void InitBootParam(void);
void ResetBootParam(void);
bool GetFactoryResult(void);
void QspiFlashWriteFF(uint32_t addr, uint32_t size);

#endif

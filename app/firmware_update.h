#ifndef _FIRMWARE_UPDATE_H
#define _FIRMWARE_UPDATE_H

#include "stdint.h"
#include "stdbool.h"
#include "err_code.h"

#define APP_ADDR        (0x1001000 + 0x80000)

#define OTA_FILE_INFO_MARK_MAX_LEN          32
#define SIGNATURE_LEN                       128

#ifdef __GNUC__
#define SIGNATURE_ENABLE        1
#define VERSION_CHECK_ENABLE    1   
#else
#define SIGNATURE_ENABLE        0
#define VERSION_CHECK_ENABLE    0
#endif

//OTA file head info.m
typedef struct {
    char mark[OTA_FILE_INFO_MARK_MAX_LEN];
    uint32_t fileSize;
    uint32_t originalFileSize;
    uint32_t crc32;
    uint32_t originalCrc32;
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
void FirmwareUpdate(char *filePath);

#endif

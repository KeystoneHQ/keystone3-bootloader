#include "firmware_update.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "user_fatfs.h"
#include "ff.h"
#include "crc.h"
#include "quicklz.h"
#include "log_print.h"
#include "drv_qspi_flash.h"
#include "cJSON.h"
#include "hal_lcd.h"
#include "user_delay.h"
#include "draw_on_lcd.h"
#include "drv_power.h"
#include "drv_lcd_bright.h"
#include "user_memory.h"
#include "sha256.h"
#include "check_app.h"
#include "mhscpu.h"
#include "drv_otp.h"
#include "user_utils.h"
#include "drv_gd25qxx.h"
#include "hal_touch.h"
#include "recovery_mode.h"

#if (SIGNATURE_ENABLE == 1)
#include "librust_c.h"
#endif

LV_FONT_DECLARE(openSans_20);
LV_FONT_DECLARE(openSans_24);

enum {
    MARK_OFFSET = 0,
    FILE_SIZE_OFFSET = 8,
    ORIGINAL_FILE_SIZE_OFFSET = 12,
    HASH_OFFSET = 16,
    ORIGINAL_HASH_OFFSET = 48,
    ENCODE_OFFSET = 80,
    ENCODE_UNIT_OFFSET = 84,
    ENCRYPT_OFFSET = 88,
    SIGNATURE_OFFSET = 92,
    ORIGINAL_SIGNATURE_OFFSET = 92 + 128,
};

typedef enum {
    CHECKSUM_SUCCESS = 0,
    CHECKSUM_INVALID_SIGNATURE = 1,
    CHECKSUM_HASH_MISMATCH = 2,
    CHECKSUM_ERROR = 3
} ChecksumResult_t;

#define FILE_MARK_MCU_FIRMWARE              "~update!"
#define FILE_MARK_MCU_FIRMWARE_2_0          "~fwdata!"

#define FILE_UNIT_SIZE                      0x4000
#define DATA_UNIT_SIZE                      0x4000

#define FIXED_SEGMENT_OFFSET                0x1000

#define UPDATE_PUB_KEY_LEN                  64
#define SD_CARD_PILLAR_PATH                 "0:pillar.bin"
#define USB_PILLAR_PATH                     "1:pillar.bin"
#define SD_CARD_KETSTONE3_PATH              "0:keystone3.bin"
#define USB_KETSTONE3_PATH                  "1:keystone3.bin"

#define SECTOR_SIZE                         4096
#define APP_ADDR                            (0x1001000 + 0x80000)   //108 1000
#define APP_CHECK_START_ADDR                (0x1400000)
#define APP_END_ADDR                        (0x2000000)

static uint8_t g_fileUnit[FILE_UNIT_SIZE + 16];
static uint8_t g_dataUnit[DATA_UNIT_SIZE];

static uint32_t BytesToUint32BE(uint8_t *bytes);
static bool IsHexChar(char c);
static uint32_t GetOtaFileInfo(OtaFileInfo_t *info, const char *filePath);
static int32_t CheckOtaFile(OtaFileInfo_t *info, const char *filePath, uint32_t *pHeadSize);
static bool CheckVersion(const OtaFileInfo_t *info, const char *filePath, uint32_t headSize);
static int32_t UpdateFromOtaFile(const OtaFileInfo_t *info, const char *filePath, uint32_t headSize);
static int32_t GetIntValue(const cJSON *obj, const char *key);
static void GetStringValue(const cJSON *obj, const char *key, char *value, uint32_t maxLen);
#if (SIGNATURE_ENABLE == 1)
static void GetSignatureValue(const cJSON *obj, char *output, uint32_t maxLength);
static void GetUpdatePubKey(uint8_t *pubKey);
const uint8_t g_defaultPubKey[] = {
    0xD9, 0xA5, 0xDB, 0x68, 0x66, 0x36, 0x4B, 0x7F, 0x55, 0xCF, 0x6F, 0x3C, 0x19, 0x9A, 0x96, 0x26,
    0x5C, 0x6E, 0x71, 0x70, 0x87, 0xBE, 0x9D, 0xA8, 0xF4, 0x1D, 0xEA, 0xF5, 0x70, 0xBC, 0x7C, 0x2E,
    0x0D, 0x48, 0x4C, 0xB3, 0x9F, 0x0D, 0xDE, 0xFF, 0xB4, 0x17, 0xF9, 0x95, 0xF9, 0x14, 0x06, 0xCB,
    0xF0, 0xE1, 0x56, 0x63, 0x9A, 0xD8, 0x05, 0x6D, 0x0E, 0xE3, 0x51, 0xC2, 0x58, 0x31, 0xF8, 0xD9
};
#endif

typedef enum {
    UPDATE_TO_FACTORY_BIN,
    UPDATE_TO_APP_BIN,
    UPDATE_BIN_BUTT,
} UpdateType_t;

void CopyBin2Flash(void)
{
#ifndef __GNUC__
    return;
#endif
    int32_t ret;
    uint8_t updateType = UPDATE_BIN_BUTT;

    if (CheckApp() == false) {
        updateType = UPDATE_TO_FACTORY_BIN;
    } else if (CheckAppFactory()) {
        updateType = UPDATE_TO_APP_BIN;
    } else {
        return;
    }

    OpenPower(POWER_TYPE_VCC33);
    UserDelay(100);
    // MountSdFatfs();
    if (FR_OK != MountSdFatfs()) {
        return;
    }
    LcdCheck();
    LcdInit();
    UserDelay(100);
    SetLcdBright(70);
    if (updateType == UPDATE_TO_FACTORY_BIN) {
        if (FatfsFileExist(SD_CARD_PILLAR_PATH)) {
            DrawStringOnLcd(140, 480, "Copying  pillar", 0xFFFF, &openSans_24);
            ret = FatfsFileCopy(SD_CARD_PILLAR_PATH, USB_PILLAR_PATH);
            if (ret == FR_OK) {
                printf("copy pillar.bin to usb success\r\n");
            }
        }
    } else if (updateType == UPDATE_TO_APP_BIN) {
        if (FatfsFileExist(SD_CARD_KETSTONE3_PATH)) {
            DrawStringOnLcd(140, 480, "Copying  keystone3", 0xFFFF, &openSans_24);
            ret = FatfsFileCopy(SD_CARD_KETSTONE3_PATH, USB_KETSTONE3_PATH);
            if (ret == FR_OK) {
                printf("copy keystone3.bin to usb success\r\n");
            }
        }
    }
}

static void FirmwareUpdateErrorHandel(Error_Code errCode)
{
    char buff[32];
    uint32_t c = 0x666666;
    LcdFullScreen(0);
    uint16_t color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
    switch (errCode) {
    case ERR_UPDATE_CHECK_CRC_FAILED:
        c = 0xF55831;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawStringOnLcd(130, 317, "Firmware Damaged", color, &openSans_24);
        DrawStringOnLcd(73, 369, "The current firmware is incomplete.\nRe-download and retry the upgrade", 0xFFFF, &openSans_20);
        break;
    case ERR_UPDATE_CHECK_SIGNATURE_FAILED:
        c = 0xF55831;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawStringOnLcd(90, 302, "Firmware Verification Failed", color, &openSans_24);
        DrawStringOnLcd(98, 354, "Firmware signature mismatch.", 0xFFFF, &openSans_20);
        DrawStringOnLcd(64, 384, "Download from the legitimate source:", 0xFFFF, &openSans_20);
        c = 0x1BE0C6;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawStringOnLcd(115, 426, "https://keyst.one/firmware.", color, &openSans_20);
        break;
    case ERR_UPDATE_CHECK_VERSION_FAILED:
        c = 0xF55831;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawStringOnLcd(160, 323, "Lower Version", color, &openSans_24);
        DrawStringOnLcd(36, 375, "Make sure the new firmware version is higher", 0xFFFF, &openSans_20);
        DrawStringOnLcd(90, 405, "than the current one on the device.", 0xFFFF, &openSans_20);
        break;
    case ERR_UPDATE_CHECK_FILE_MARK_FAILED:
        c = 0xF55831;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawStringOnLcd(160, 323, "Lower Version", color, &openSans_24);
        DrawStringOnLcd(45, 375, "Make sure the version is higher than 2.0", color, &openSans_20);
        DrawStringOnLcd(90, 405, "Please download the firmware\n         version 2.0 or higher", 0xFFFF, &openSans_20);
        break;
    default:
        break;
    }
    for (int i = 0; i < 9; i++) {
        sprintf(buff, "%d", 9 - i);
        DrawStringOnLcd(234, 463, buff, 0xFFFF, &openSans_24);
        UserDelay(1000);
    }
}

/// @brief Update firmware from file stored in USB mass storage device.
/// @param
void FirmwareUpdate(char *filePath)
{
    int32_t ret = SUCCESS_CODE;
    OtaFileInfo_t otaFileInfo = {0};
    uint32_t c = 0x666666;
    uint16_t color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
    uint32_t headSize;

    ret = CheckOtaFile(&otaFileInfo, filePath, &headSize);
    if (ret != SUCCESS_CODE) {
        if (ret == ERR_UPDATE_CHECK_FILE_EXIST) {
            return;
        }
        LcdOpen();
        printf("file delete\n");
        f_unlink(filePath);
        FirmwareUpdateErrorHandel(ret);
        return;
    }

#if (VERSION_CHECK_ENABLE == 1)
    if (CheckVersion(&otaFileInfo, filePath, headSize) == false) {
        printf("file %s version err\n", filePath);
        f_unlink(filePath);
        LcdOpen();
        FirmwareUpdateErrorHandel(ERR_UPDATE_CHECK_VERSION_FAILED);
        return;
    }
#endif
    printf("start to update firmware,file %s\n", filePath);
    SetLcdBright(0);
    OpenPower(POWER_TYPE_VCC33);
    LcdCheck();
    LcdInit();
    c = 0x666666;
    color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
    DrawStringOnLcd(56, 460, "Please Keep the Device ON and Maintain", color, &openSans_20);
    DrawStringOnLcd(175, 490, "Power Supply", color, &openSans_20);
    UserDelay(100);
    SetLcdBright(70);
    ret = UpdateFromOtaFile(&otaFileInfo, filePath, headSize);
    f_unlink(filePath);
    if (ret != SUCCESS_CODE) {
        FirmwareUpdateErrorHandel(ret);
        return;
    }
    NVIC_SystemReset();
}


/// @brief
/// @param info
/// @param filePath
/// @return head size, as same as data index.
static uint32_t GetOtaFileInfo(OtaFileInfo_t *info, const char *filePath)
{
    FIL fp;
    int32_t ret;
    uint32_t headSize = 0, readSize;
    char *headJsonStr = NULL;
    cJSON *jsonRoot;

    ret = f_open(&fp, filePath, FA_OPEN_EXISTING | FA_READ);
    do {
        if (ret) {
            FatfsError((FRESULT)ret);
            break;
        }
        ret = f_read(&fp, &headSize, 4, (UINT *)&readSize);
        if (ret) {
            FatfsError((FRESULT)ret);
            break;
        }
        printf("headSize=%d\r\n", headSize);
        headJsonStr = pvPortMalloc(headSize + 1);
        if (headJsonStr == NULL) {
            printf("malloc err\r\n");
            break;
        }
        ret = f_read(&fp, headJsonStr, headSize, (UINT *)&readSize);
        if (ret) {
            FatfsError((FRESULT)ret);
            break;
        }
        if (readSize != headSize) {
            printf("read err,readSize=%d,headSize=%d\r\n", readSize, headSize);
            break;
        }
        memcpy(info->mark, headJsonStr, 8);
        if (strcmp(info->mark, FILE_MARK_MCU_FIRMWARE_2_0) != 0) {
            printf("file info mark err\r\n");
            if (strcmp(info->mark, FILE_MARK_MCU_FIRMWARE) == 0) {
                return 0;
            }
            return 0;
        }
        info->fileSize = BytesToUint32BE(headJsonStr + FILE_SIZE_OFFSET);
        info->originalFileSize = BytesToUint32BE(headJsonStr + ORIGINAL_FILE_SIZE_OFFSET);
        memcpy(info->hash, headJsonStr + HASH_OFFSET, 32);
        memcpy(info->originalHash, headJsonStr + ORIGINAL_HASH_OFFSET, 32);
        info->encode = BytesToUint32BE(headJsonStr + ENCODE_OFFSET);
        info->encodeUnit = BytesToUint32BE(headJsonStr + ENCODE_UNIT_OFFSET);
        info->encrypt = BytesToUint32BE(headJsonStr + ENCRYPT_OFFSET);
        printf("info->fileSize=%d\r\n", info->fileSize);
        printf("info->originalFileSize=%d\r\n", info->originalFileSize);
        PrintArray("info->hash", info->hash, 32);
        PrintArray("info->originalHash", info->originalHash, 32);
        printf("info->encode=%d\r\n", info->encode);
        printf("info->encodeUnit=%d\r\n", info->encodeUnit);
        printf("info->encrypt=%d\r\n", info->encrypt);
        memcpy(info->signature, headJsonStr + SIGNATURE_OFFSET, SIGNATURE_LEN);
        printf("info->signature=%s\r\n", info->signature);
        memcpy(info->originalSignature, headJsonStr + ORIGINAL_SIGNATURE_OFFSET, SIGNATURE_LEN);
        printf("info->originalSignature=%s\r\n", info->originalSignature);
    } while (0);
    if (headJsonStr != NULL) {
        vPortFree(headJsonStr);
    }
    f_close(&fp);
    return headSize + 5;    //4 byte uint32 and 1 byte json string '\0' end.
}

static int32_t CheckOtaFile(OtaFileInfo_t *info, const char *filePath, uint32_t *pHeadSize)
{
    FIL fp;
    int32_t ret;
    uint32_t fileSize, crcCalc, readSize, i, headSize, j;
    int32_t bRet = SUCCESS_CODE;
    sha256_context ctx;

    headSize = GetOtaFileInfo(info, filePath);
    if (headSize == 0) {
        return ERR_UPDATE_CHECK_FILE_MARK_FAILED;
    }
    *pHeadSize = headSize;
    ret = f_open(&fp, filePath, FA_OPEN_EXISTING | FA_READ);
    if (ret) {
        FatfsError((FRESULT)ret);
        return ERR_UPDATE_CHECK_FILE_EXIST;
    }
    fileSize = f_size(&fp);
    printf("mark=%s\r\n", info->mark);
    printf("fileSize=%d\r\n", info->fileSize);
    printf("originalFileSize=%d\r\n", info->originalFileSize);
    PrintArray("hash", info->hash, 32);
    PrintArray("originalHash", info->originalHash, 32);
    printf("encode=%d\r\n", info->encode);
    printf("encodeUnit=%d\r\n", info->encodeUnit);
    printf("encrypt=%d\r\n", info->encrypt);
    printf("destPath=%s\r\n", info->destPath);
    printf("originalBriefSize=%d\r\n", info->originalBriefSize);
    printf("originalBriefCrc32=0x%08X\r\n", info->originalBriefCrc32);
#if (SIGNATURE_ENABLE == 1)
    printf("signature=%s\r\n", info->signature);
#endif
    bRet = SUCCESS_CODE;
    do {
        if (fileSize != info->fileSize + headSize) {
            printf("file size err,fileSize=%d, info->fileSize=%d\r\n", fileSize, info->fileSize);
            bRet = ERR_UPDATE_CHECK_CRC_FAILED;
            break;
        }
        printf("start to check file crc32\r\n");
        f_lseek(&fp, headSize);

        sha256_init(&ctx);
        uint8_t content_hash[32];
        for (i = headSize; i < fileSize; i += readSize) {
            ret = f_read(&fp, &g_fileUnit, FILE_UNIT_SIZE, (UINT *)&readSize);
            if (ret) {
                FatfsError((FRESULT)ret);
                bRet = ERR_UPDATE_CHECK_CRC_FAILED;
                break;
            }
            sha256_hash(&ctx, g_fileUnit, readSize);
        }
        sha256_done(&ctx, content_hash);
        PrintArray("hash content:", content_hash, 32);
        if (memcmp(content_hash, info->hash, 32) != 0) {
            PrintArray("hash content:", content_hash, 32);
            PrintArray("hash info:", info->hash, 32);
            bRet = ERR_UPDATE_CHECK_CRC_FAILED;
            break;
        }

#if (SIGNATURE_ENABLE == 1)
        printf("signature=%s\r\n", info->signature);
        if (strlen(info->signature) != 128) {
            printf("error signature=%s\r\n", info->signature);
            bRet = ERR_UPDATE_CHECK_SIGNATURE_FAILED;
            break;
        }
        // signature number check is not 0-f
        for (j = 0; j < 128; j++) {
            if (!IsHexChar(info->signature[j])) {
                break;
            }
        }
        if (j != 128) {
            bRet = ERR_UPDATE_CHECK_SIGNATURE_FAILED;
            break;
        }
        // TODO: find this public key from firmware section.
        uint8_t publickey[65] = {0};
        GetUpdatePubKey(publickey);
        PrintArray("pubKey", publickey, 65);
        if (verify_frimware_signature(info->signature, content_hash, publickey) != true) {
            printf("signature check error\n");
            bRet = ERR_UPDATE_CHECK_SIGNATURE_FAILED;
            break;
        }
#endif
    } while (0);
    f_close(&fp);

    return bRet;
}


static bool CheckVersion(const OtaFileInfo_t *info, const char *filePath, uint32_t headSize)
{
    FIL fp;
    int32_t ret;
    uint32_t readSize, cmpsdSize, decmpsdSize;
    qlz_state_decompress qlzState = {0};
    uint32_t nowMajor, nowMinor, nowBuild;
    uint32_t fileMajor, fileMinor, fileBuild;

    ret = f_open(&fp, filePath, FA_OPEN_EXISTING | FA_READ);
    if (ret) {
        FatfsError((FRESULT)ret);
        return false;
    }
    f_lseek(&fp, headSize);
    ret = f_read(&fp, g_fileUnit, 16, (UINT *)&readSize);
    if (ret) {
        FatfsError((FRESULT)ret);
        f_close(&fp);
        return false;
    }
    cmpsdSize = qlz_size_compressed((char*)g_fileUnit);
    decmpsdSize = qlz_size_decompressed((char*)g_fileUnit);
    printf("cmpsdSize=%d,decmpsdSize=%d\r\n", cmpsdSize, decmpsdSize);
    ret = f_read(&fp, g_fileUnit + 16, cmpsdSize - 16, (UINT *)&readSize);
    if (ret) {
        FatfsError((FRESULT)ret);
        f_close(&fp);
        return false;
    }
    qlz_decompress((char*)g_fileUnit, g_dataUnit, &qlzState);
    GetSoftwareVersion(&nowMajor, &nowMinor, &nowBuild);
    GetSoftwareVersionFormData(&fileMajor, &fileMinor, &fileBuild, g_dataUnit + FIXED_SEGMENT_OFFSET, decmpsdSize - FIXED_SEGMENT_OFFSET);
    printf("now version:%d.%d.%d\n", nowMajor, nowMinor, nowBuild);
    printf("file version:%d.%d.%d\n", fileMajor, fileMinor, fileBuild);

    // each valid number should be from (0~99)
    uint32_t epoch = 100;
    uint32_t nowVersionNumber = (nowMajor * epoch * epoch)  + (nowMinor * epoch) + nowBuild;
    uint32_t fileVersionNumber = (fileMajor * epoch * epoch)  + (fileMinor * epoch) + fileBuild;

    if ((nowMajor == 99 && nowMinor == 99 && nowBuild == 99)) {
        return true;
    } else if (fileVersionNumber > nowVersionNumber) {
        return true;
    } else if (fileVersionNumber == nowVersionNumber) {
        return CalculateCheckSum(true, info->originalHash);
        // } else if (fileMajor == 0 && fileMinor  == 0 && fileBuild == 1) {
        //     return true;
    } else {
        return false;
    }
}

static int32_t ReadOtaFileAndCheck(const OtaFileInfo_t *info, const char *filePath, uint32_t headSize, bool write)
{
    FIL fp;
    int32_t ret;
    uint32_t fileSize, readSize, i, offset, cmpsdSize, decmpsdSize, writeAddr, percent;
    sha256_context ctx;
    qlz_state_decompress qlzState = {0};
    static uint32_t lastPercent = 101;
    char percentStr[16];
    uint8_t content_hash[32];

    if (write) {
        DrawStringOnLcd(190, 412, "Installing", 0xFFFF, &openSans_24);
        DrawStringOnLcd(215, 620, "               ", 0xFFFF, &openSans_24);
    } else {
        DrawStringOnLcd(190, 412, "Checking", 0xFFFF, &openSans_24);
    }
    DrawStringOnLcd(215, 620, "0%", 0xFFFF, &openSans_24);
    DrawProgressBarOnLcd(80, 594, 320, 9, 0, 0x21F4);
    ret = f_open(&fp, filePath, FA_OPEN_EXISTING | FA_READ);
    if (ret) {
        FatfsError((FRESULT)ret);
        return ERR_UPDATE_CHECK_FILE_EXIST;
    }
    sha256_init(&ctx);

    fileSize = f_size(&fp);
    f_lseek(&fp, headSize);
    writeAddr = APP_ADDR;
    for (i = headSize; i < fileSize;) {
        ret = f_read(&fp, g_fileUnit, 16, (UINT *)&readSize);
        if (ret) {
            FatfsError((FRESULT)ret);
            f_close(&fp);
            return ERR_UPDATE_CHECK_FILE_EXIST;
        }
        i += readSize;
        cmpsdSize = qlz_size_compressed((char*)g_fileUnit);
        decmpsdSize = qlz_size_decompressed((char*)g_fileUnit);
        ret = f_read(&fp, g_fileUnit + 16, cmpsdSize - 16, (UINT *)&readSize);
        if (ret) {
            FatfsError((FRESULT)ret);
            f_close(&fp);
            return ERR_UPDATE_CHECK_FILE_EXIST;
        }
        qlz_decompress((char*)g_fileUnit, g_dataUnit, &qlzState);
        sha256_hash(&ctx, g_dataUnit, decmpsdSize);
        for (offset = 0; offset < decmpsdSize; offset += 4096) {
            if (write) {
                QspiFlashEraseAndWrite(writeAddr, g_dataUnit + offset, 4096);
            }
            writeAddr += 4096;
        }
        i += readSize;
        percent = i * 100 / fileSize;
        if (percent != lastPercent) {
            printf("%d%%...\r\n", percent);
            sprintf(percentStr, "%d%%", percent);
            DrawStringOnLcd(215, 620, percentStr, 0xFFFF, &openSans_24);
            DrawProgressBarOnLcd(80, 594, 320, 9, percent, 0x21F4);
            lastPercent = percent;
        }
    }

    sha256_done(&ctx, content_hash);
    PrintArray("content_hash", content_hash, 32);
    PrintArray("originalHash", info->originalHash, 32);
    uint8_t publickey[65] = {0};
    GetUpdatePubKey(publickey);
    if (memcmp(content_hash, info->originalHash, 32) == 0) {
        if (verify_frimware_signature(info->originalSignature, content_hash, publickey)) {
            printf("check signature ok\r\n");
            char *signature = pvPortMalloc(4096);
            memcpy(signature, info->originalSignature, sizeof(info->originalSignature));
            if (write) {
                QspiFlashEraseAndWrite(APP_END_ADDR - 4096, signature, 4096);
            }
            printf("signature:%s\r\n", signature);
            memset(signature, 0, 4096);
            vPortFree(signature);
            return SUCCESS_CODE;
        } else {
            printf("check signature failed\r\n");
            return ERR_UPDATE_CHECK_SIGNATURE_FAILED;
        }
    } else {
        printf("update failed\r\n");
        char *signature = pvPortMalloc(4096);
        for (int i = 0; i < 256; i++) {
            signature[i] = i;
        }
        if (write) {
            QspiFlashEraseAndWrite(APP_END_ADDR - 4096, signature, 4096);
        }
        vPortFree(signature);
        return ERR_UPDATE_CHECK_CRC_FAILED;
    }
    return SUCCESS_CODE;
}

static int32_t UpdateFromOtaFile(const OtaFileInfo_t *info, const char *filePath, uint32_t headSize)
{
    int32_t ret = ReadOtaFileAndCheck(info, filePath, headSize, false);
    if (ret != SUCCESS_CODE) {
        return ret;
    }
    return ReadOtaFileAndCheck(info, filePath, headSize, true);
}

/**
 * @brief       Get integer value from cJSON object.
 * @param[in]   obj : cJSON object.
 * @param[in]   key : key name.
 * @retval      integer value to get.
 */
static int32_t GetIntValue(const cJSON *obj, const char *key)
{
    cJSON *intJson = cJSON_GetObjectItem((cJSON *)obj, key);
    if (intJson != NULL) {
        return (uint32_t)intJson->valuedouble;
    }
    printf("key:%s does not exist\r\n", key);
    return 0;
}


/**
 * @brief       Get string value from cJSON object.
 * @param[in]   obj : cJSON object.
 * @param[in]   key : key name.
 * @param[out]  value : return string value, if the acquisition fails, the string will be cleared.
 * @retval
 */
static void GetStringValue(const cJSON *obj, const char *key, char *value, uint32_t maxLen)
{
    cJSON *json;
    uint32_t len;
    char *strTemp;

    json = cJSON_GetObjectItem((cJSON *)obj, key);
    if (json != NULL) {
        strTemp = json->valuestring;
        len = strlen(strTemp);
        if (len < maxLen) {
            strcpy(value, strTemp);
        } else {
            strcpy(value, "");
        }
    } else {
        printf("key:%s does not exist\r\n", key);
        strcpy(value, "");
    }
}

#if (SIGNATURE_ENABLE == 1)
static void GetSignatureValue(const cJSON *obj, char *output, uint32_t maxLength)
{
    cJSON *signatureValue = cJSON_GetObjectItem((cJSON *)obj, "signature");
    if (signatureValue->type == cJSON_String) {
        char *strTemp = signatureValue->valuestring;
        strncpy(output, strTemp, maxLength);
        return;
    }
    memset(output, 0, maxLength);
    printf("signature does not exist\r\n");
}

static void GetUpdatePubKey(uint8_t *pubKey)
{
    uint8_t data[UPDATE_PUB_KEY_LEN];
    uint32_t addr;

    pubKey[0] = 4;
    OTP_PowerOn();
    for (addr = OTP_ADDR_UPDATE_PUB_KEY + 1024 - UPDATE_PUB_KEY_LEN; addr >= OTP_ADDR_UPDATE_PUB_KEY; addr -= UPDATE_PUB_KEY_LEN) {
        memcpy(data, (uint8_t *)addr, UPDATE_PUB_KEY_LEN);
        //PrintArray("read pub key", data, UPDATE_PUB_KEY_LEN);
        if (CheckAllFF(data, UPDATE_PUB_KEY_LEN) == false) {
            if (CheckEntropy(data, UPDATE_PUB_KEY_LEN)) {
                //Found
                printf("found,addr=0x%X\n", addr);
                memcpy(pubKey + 1, data, UPDATE_PUB_KEY_LEN);
                memset(data, 0, UPDATE_PUB_KEY_LEN);
                return;
            }
            printf("default public key\n");
            memset(data, 0, UPDATE_PUB_KEY_LEN);
            memcpy(pubKey + 1, g_defaultPubKey, sizeof(g_defaultPubKey));
            return;
        }
    }
    printf("default public key\n");
    memset(data, 0, UPDATE_PUB_KEY_LEN);
    memcpy(pubKey + 1, g_defaultPubKey, sizeof(g_defaultPubKey));
}

static bool VerifyFirmwareSignature(const uint8_t *hash)
{
    uint8_t publickey[65] = {0};
    GetUpdatePubKey(publickey);

    char *signature = pvPortMalloc(256 + 1);
    if (signature == NULL) {
        return false;
    }

    memcpy(signature, APP_END_ADDR - 4096, 256);
    bool isOK = verify_frimware_signature(signature, hash, publickey);
    vPortFree(signature);

    return isOK;
}
#endif

#if 1
static uint32_t BinarySearchLastNonFFSector(void)
{
    uint8_t *buffer = pvPortMalloc(SECTOR_SIZE);
    uint32_t startIndex = (APP_CHECK_START_ADDR - APP_ADDR) / SECTOR_SIZE;
    uint32_t endIndex = (APP_END_ADDR - APP_ADDR) / SECTOR_SIZE;

    uint8_t percent = 1;

    for (int i = startIndex + 1; i < endIndex; i++) {
        memcpy(buffer, (uint32_t *)(APP_ADDR + i * SECTOR_SIZE), SECTOR_SIZE);
        if ((i - startIndex) % 200 == 0) {
            percent++;
        }
        if (CheckAllFF(&buffer[2], SECTOR_SIZE - 2) && ((buffer[0] * 256 + buffer[1]) < 4096)) {
            vPortFree(buffer);
            return i;
        }
    }
    vPortFree(buffer);
    return -1;
}

void NotToDuFunc(void)
{

}

static bool CalculateFirmwareHash(uint8_t *hash, bool showProgress)
{
    uint8_t buffer[SECTOR_SIZE] = {0};
    char percentStr[16] = {0};
    uint8_t percent = 0;

    int lastSector = BinarySearchLastNonFFSector();
    if (lastSector < 0) {
        return false;
    }

    sha256_context ctx;
    sha256_init(&ctx);
    if (showProgress) {
        DrawStringOnLcd(155, 412, "Check firmware", 0xFFFF, &openSans_24);
    }

    for (int i = 0; i <= lastSector; i++) {
        memset(buffer, 0, SECTOR_SIZE);
        memcpy(buffer, (uint32_t *)(APP_ADDR + i * SECTOR_SIZE), SECTOR_SIZE);
        sha256_hash(&ctx, buffer, SECTOR_SIZE);

        if (showProgress) {
            percent = i * 100 / lastSector;
            snprintf(percentStr, sizeof(percentStr), "%d%%", percent);
            DrawStringOnLcd(215, 620, percentStr, 0xFFFF, &openSans_24);
            DrawProgressBarOnLcd(80, 594, 320, 9, percent, 0x21F4);
        }
    }

    sha256_done(&ctx, hash);
    memset(buffer, 0, SECTOR_SIZE);
    return true;
}

static void HandleFirmwareVerificationFailed(void)
{
    ReducedGlInit();
    SimpleDrawButton(100, 500, 280, 60, _COLOR_MAKE(0xFF, 0, 0), "firmware not secure");

    for (int i = 9; i > 0; i--) {
        char buff[32];
        snprintf(buff, sizeof(buff), "Wipe Device %d", i);
        SimpleDrawButton(180, 423, 130, 60, _COLOR_MAKE(0, 0, 0), buff);
        UserDelay(1000);
    }
    WipeDeviceCallbackFunc();
}

bool CalculateCheckSum(bool checkSum, const uint8_t *originalHash)
{
    if (CheckAppFactory()) {
        return true;
    }

    uint8_t hash[32] = {0};
    do {
        if (!checkSum) {
            LcdOpen();
        }

        if (!CalculateFirmwareHash(hash, !checkSum)) {
            break;
        }

        PrintArray("hash", hash, 32);
        if (checkSum && originalHash != NULL) {
            PrintArray("originalHash", originalHash, 32);
            return (memcmp(hash, originalHash, 32) != 0);
        }
    } while (0);

    if (!checkSum) {
        LcdFullScreen(0);

#if SIGNATURE_ENABLE
        if (!VerifyFirmwareSignature(hash)) {
            HandleFirmwareVerificationFailed();
            return false;
        }
#endif
    }

    return true;
}
#endif

static uint32_t BytesToUint32BE(uint8_t *bytes)
{
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | (uint32_t)bytes[3];
}

static bool IsHexChar(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "user_fatfs.h"
#include "firmware_update.h"
#include "ff.h"
#include "crc.h"
#include "quicklz.h"
#include "log_print.h"
#include "drv_qspi_flash.h"
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
#include "ctaes.h"
#include "reduced_gl.h"
#include "imgWarn.h"
#include "imgInstall.h"
#include "imgSecurity.h"
#include "imgDamaged.h"

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

typedef struct {
    // boot check
    uint8_t bootCheckFlag[16];

    // recovery mode switch
    uint8_t recoveryModeSwitch[16];
} BootParam_t;
static BootParam_t g_bootParam;

typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t build;
} VersionInfo_t;

#define FILE_MARK_MCU_FIRMWARE "~update!"
#define FILE_MARK_MCU_FIRMWARE_2_0 "~fwdata!"

#define FILE_UNIT_SIZE 0x4000
#define DATA_UNIT_SIZE 0x4000

#define FIXED_SEGMENT_OFFSET 0x1000

#define UPDATE_PUB_KEY_LEN 64
#define SD_CARD_PILLAR_PATH                     "0:pillar.bin"
#define SD_F_CARD_KETSTONE3_PATH                "0:f_keystone3.bin"

#define SECTOR_SIZE                                         4096
#define APP_ADDR                                            (0x1001000 + 0x80000) // 108 1000
#define APP_CHECK_START_ADDR                                (0x1400000)
#define APP_END_ADDR                                        (0x2000000)
#define APP_SIGNATURE_ADDR                                  (APP_END_ADDR - 4096)
#define APP_FLASH_START_ADDR                                (APP_ADDR)
#define APP_FLASH_END_ADDR                                  (APP_SIGNATURE_ADDR)
#define SRAM_START_ADDR                                     (0x20000000)
#define SRAM_END_ADDR                                       (0x20100000)
#define BOOT_SECURE_PARAM_FLAG                              (0x00F6D800)
#define BOOT_SECURE_PARAM_FLAG_SIZE                         (0x1000)
#define OTA_ADDR_FACTORY_BASE                               (0x40009400)
#define VERSION_COMPONENT_MAX                               (99U)

static uint8_t g_fileUnit[FILE_UNIT_SIZE + 16];
static uint8_t g_dataUnit[DATA_UNIT_SIZE];
static bool g_shouldWriteOtpVersionHistory = false;
static VersionInfo_t g_pendingOtpVersion = {0};

static uint32_t BytesToUint32BE(char *bytes);
static bool IsHexChar(char c);
static bool IsAddressInRange(uint32_t addr, uint32_t start, uint32_t end);
static bool IsValidAppVector(uint32_t msp, uint32_t resetHandler);
static uint32_t GetOtaFileInfo(OtaFileInfo_t *info, const char *filePath);
static int32_t CheckOtaFile(OtaFileInfo_t *info, const char *filePath, uint32_t *pHeadSize);
static bool CheckVersion(const OtaFileInfo_t *info, const char *filePath, uint32_t headSize);
static int32_t UpdateFromOtaFile(const OtaFileInfo_t *info, const char *filePath, uint32_t headSize);
static bool IsValidVersionComponent(uint32_t major, uint32_t minor, uint32_t build);
static uint32_t BuildVersionNumber(uint32_t major, uint32_t minor, uint32_t build);
static int32_t CompareVersion(VersionInfo_t lhs, VersionInfo_t rhs);
static uint32_t PackVersionToWord(VersionInfo_t version);
static VersionInfo_t UnpackVersionFromWord(uint32_t word);
bool GetLatestOtpHistoryVersion(VersionInfo_t *version, uint32_t *lastAddr, uint32_t *nextAddr);
static void TryWriteVersionHistoryToOtp(void);
static void AesEncryptBuffer(uint8_t *cipher, uint32_t sz, uint8_t *plain);
void AesDecryptBuffer(uint8_t *plain, uint32_t sz, uint8_t *cipher);
int SaveBootParam(void);

#if (SIGNATURE_ENABLE == 1)
static void GetUpdatePubKey(uint8_t *pubKey);
const uint8_t g_defaultPubKey[] = {
    0xD9, 0xA5, 0xDB, 0x68, 0x66, 0x36, 0x4B, 0x7F, 0x55, 0xCF, 0x6F, 0x3C, 0x19, 0x9A, 0x96, 0x26,
    0x5C, 0x6E, 0x71, 0x70, 0x87, 0xBE, 0x9D, 0xA8, 0xF4, 0x1D, 0xEA, 0xF5, 0x70, 0xBC, 0x7C, 0x2E,
    0x0D, 0x48, 0x4C, 0xB3, 0x9F, 0x0D, 0xDE, 0xFF, 0xB4, 0x17, 0xF9, 0x95, 0xF9, 0x14, 0x06, 0xCB,
    0xF0, 0xE1, 0x56, 0x63, 0x9A, 0xD8, 0x05, 0x6D, 0x0E, 0xE3, 0x51, 0xC2, 0x58, 0x31, 0xF8, 0xD9
};
#endif
static const uint8_t g_integrityFlag[16] = {
    0x01, 0x09, 0x00, 0x03,
    0x01, 0x09, 0x00, 0x03,
    0x01, 0x09, 0x00, 0x03,
    0x01, 0x09, 0x00, 0x03,
};
static const uint8_t g_recoveryModeFlag[16] = {
    'r', 'e', 'c', 'o',
    'v', 'e', 'r', 'y',
    'm', 'o', 'd', 'e',
    'f', 'l', 'a', 'g',
};
typedef enum {
    UPDATE_TO_FACTORY_BIN,
    UPDATE_FACTORY_TO_APP_BIN,
    UPDATE_APP_TO_APP_BIN,
    UPDATE_BIN_BUTT,
} UpdateType_t;

Error_Code CopyBin2Flash(bool app2app)
{
    int32_t ret;
    UpdateType_t updateType = UPDATE_BIN_BUTT;
    const char *srcPath = NULL;
    const char *showText = NULL;
    const char *fileName = NULL;

    if (CheckApp() == false) {
        updateType = UPDATE_TO_FACTORY_BIN;
    } else if (CheckAppFactory()) {
        updateType = UPDATE_FACTORY_TO_APP_BIN;
    } else if (app2app) {
        updateType = UPDATE_APP_TO_APP_BIN;
    } else {
        return ERR_UPDATE_INVALID_TYPE;
    }

    switch (updateType) {
    case UPDATE_TO_FACTORY_BIN:
        srcPath = SD_CARD_PILLAR_PATH;
        showText = "Copying  pillar";
        fileName = "pillar.bin";
        break;
    case UPDATE_FACTORY_TO_APP_BIN:
        srcPath = SD_F_CARD_KETSTONE3_PATH;
        showText = "Copying  keystone3";
        fileName = "f_keystone3.bin";
        break;
    case UPDATE_APP_TO_APP_BIN:
        srcPath = SD_CARD_KEYSTONE3_PATH;
        showText = "Copying  keystone3";
        fileName = "keystone3.bin";
        break;
    default:
        return ERR_UPDATE_INVALID_TYPE;
    }

    if (updateType != UPDATE_APP_TO_APP_BIN) {
        OpenPower(POWER_TYPE_VCC33);
        UserDelay(100);
        LcdCheck();
        LcdInit();
        UserDelay(100);
        SetLcdBright(70);
    }
    
    if (FR_OK != MountSdFatfs()) {
        printf("MountSdFatfs failed\r\n");
        return ERR_UPDATE_MOUNT_FAILED;
    }

    if (!FatfsFileExist(srcPath)) {
        printf("source file not found: %s\r\n", srcPath);
        return ERR_UPDATE_CHECK_FILE_EXIST;
    }

    DrawStringOnLcd(140, 346, showText, 0xFFFF, &openSans_24);
    DrawStringOnLcd(50, 394, "Do not remove the MicroSD Card while the", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
    DrawStringOnLcd(158, 434, "update is underway.", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
    ret = FatfsFileCopy(srcPath, UPDATE_KEYSTONE3_PATH);
    if (ret != FR_OK) {
        FatfsError((FRESULT)ret);
        printf("copy %s to usb failed, ret=%d\r\n", fileName, ret);
        return ERR_UPDATE_CHECK_FILE_EXIST;
    }

    printf("copy %s to device success\r\n", fileName);
    return SUCCESS_CODE;
}

static void FirmwareUpdateErrorHandel(Error_Code errCode)
{
    char buff[32];
    uint32_t c = 0x666666;
    LcdFullScreen(0);
    uint16_t height = 0;
    uint16_t color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
    switch (errCode) {
    case ERR_UPDATE_CHECK_CRC_FAILED:
        c = 0xF55831;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawRectPic(204, 277, IMGDAMAGED_HEIGHT, IMGDAMAGED_WIDTH, imgDamaged);
        DrawStringOnLcd(130, 381, "Firmware Damaged", color, &openSans_24);
        DrawStringOnLcd(73, 431, "The current firmware is incomplete,", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        DrawStringOnLcd(63, 461, "damaged, or has been tampered with.", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        DrawStringOnLcd(40, 491, "Please re-download and retry the upgrade.", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        DrawStringOnLcd(64, 668, "Download from the legitimate source:", 0xFFFF, &openSans_20);
        c = 0x1BE0C6;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawStringOnLcd(115, 706, "https://keyst.one/firmware", color, &openSans_20);
        height = 600;
        break;
    case ERR_UPDATE_CHECK_SIGNATURE_FAILED:
        c = 0xF55831;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawRectPic(204, 277, IMGDAMAGED_HEIGHT, IMGDAMAGED_WIDTH, imgWarn);
        DrawStringOnLcd(90, 381, "Firmware Verification Failed", color, &openSans_24);
        DrawStringOnLcd(98, 431, "Firmware signature mismatch.", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        DrawStringOnLcd(64, 668, "Download from the legitimate source:", 0xFFFF, &openSans_20);
        c = 0x1BE0C6;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawStringOnLcd(115, 706, "https://keyst.one/firmware", color, &openSans_20);
        height = 488;
        break;
    case ERR_UPDATE_CHECK_VERSION_FAILED:
        c = 0xF55831;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawRectPic(204, 230, IMGDAMAGED_HEIGHT, IMGDAMAGED_WIDTH, imgDamaged);
        DrawStringOnLcd(160, 334, "Lower Version", color, &openSans_24);
        DrawStringOnLcd(63, 382, "· The firmware version is less than or", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        DrawStringOnLcd(73, 412, "equal to the current device version.", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        DrawStringOnLcd(63, 442, "· Please download and install the latest", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        DrawStringOnLcd(200, 472, "version.", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        height = 534;
        break;
    case ERR_UPDATE_CHECK_FILE_MARK_FAILED:
        c = 0xF55831;
        color = (uint16_t)(((c & 0xF80000) >> 16) | ((c & 0xFC00) >> 13) | ((c & 0x1C00) << 3) | ((c & 0xF8) << 5));
        DrawRectPic(204, 230, IMGDAMAGED_HEIGHT, IMGDAMAGED_WIDTH, imgDamaged);
        DrawStringOnLcd(160, 323, "Lower Version", color, &openSans_24);
        DrawStringOnLcd(45, 375, "Make sure the version is higher than 2.0", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        DrawStringOnLcd(90, 405, "Please download the firmware\n         version 2.0 or higher", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
        height = 534;
        break;
    default:
        break;
    }
    for (int i = 0; i < 9; i++) {
        sprintf(buff, "%d", 9 - i);
        DrawStringOnLcd(234, height, buff, 0xFFFF, &openSans_24);
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

    g_shouldWriteOtpVersionHistory = false;
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
    DrawStringOnLcd(56, 460, "Please keep the device on and ensure a", color, &openSans_20);
    DrawStringOnLcd(150, 490, "stable power supply.", color, &openSans_20);
    UserDelay(100);
    SetLcdBright(70);
    ret = UpdateFromOtaFile(&otaFileInfo, filePath, headSize);
    f_unlink(filePath);
    if (ret != SUCCESS_CODE) {
        FirmwareUpdateErrorHandel(ret);
        return;
    }
    if (!GetBootSecureCheckFlag()) {
        CalculateCheckSum(false, NULL);
    }
    TryWriteVersionHistoryToOtp();
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
    uint32_t dataOffset = 0;
    char *headStr = NULL;

    ret = f_open(&fp, filePath, FA_OPEN_EXISTING | FA_READ);
    do {
        if (ret) {
            FatfsError((FRESULT)ret);
            return ERR_UPDATE_CHECK_FILE_EXIST;
        }
        ret = f_read(&fp, &headSize, 4, (UINT *)&readSize);
        if (ret || readSize != 4) {
            FatfsError((FRESULT)ret);
            break;
        }
        if (headSize == 0 || headSize >= FILE_UNIT_SIZE) {
            printf("invalid headSize=%d\r\n", headSize);
            break;
        }
        printf("headSize=%d\r\n", headSize);
        headStr = pvPortMalloc(headSize + 1);
        if (headStr == NULL) {
            printf("malloc err\r\n");
            break;
        }
        ret = f_read(&fp, headStr, headSize, (UINT *)&readSize);
        if (ret) {
            FatfsError((FRESULT)ret);
            break;
        }
        if (readSize != headSize) {
            printf("read err,readSize=%d,headSize=%d\r\n", readSize, headSize);
            break;
        }
        headStr[headSize] = '\0';
        memset(info->mark, 0, sizeof(info->mark));
        memcpy(info->mark, headStr, 8);
        if (strcmp(info->mark, FILE_MARK_MCU_FIRMWARE_2_0) != 0) {
            printf("file info mark err\r\n");
            printf("info->mark=%s\r\n", info->mark);
            if (strcmp(info->mark, FILE_MARK_MCU_FIRMWARE) == 0) {
                return 0;
            }
            return 0;
        }
        info->fileSize = BytesToUint32BE(headStr + FILE_SIZE_OFFSET);
        info->originalFileSize = BytesToUint32BE(headStr + ORIGINAL_FILE_SIZE_OFFSET);
        memcpy(info->hash, headStr + HASH_OFFSET, 32);
        memcpy(info->originalHash, headStr + ORIGINAL_HASH_OFFSET, 32);
        info->encode = BytesToUint32BE(headStr + ENCODE_OFFSET);
        info->encodeUnit = BytesToUint32BE(headStr + ENCODE_UNIT_OFFSET);
        info->encrypt = BytesToUint32BE(headStr + ENCRYPT_OFFSET);
        printf("info->fileSize=%d\r\n", info->fileSize);
        printf("info->originalFileSize=%d\r\n", info->originalFileSize);
        PrintArray("info->hash", info->hash, 32);
        PrintArray("info->originalHash", info->originalHash, 32);
        printf("info->encode=%d\r\n", info->encode);
        printf("info->encodeUnit=%d\r\n", info->encodeUnit);
        printf("info->encrypt=%d\r\n", info->encrypt);
        memset(info->signature, 0, sizeof(info->signature));
        memcpy(info->signature, headStr + SIGNATURE_OFFSET, SIGNATURE_LEN);
        printf("info->signature=%s\r\n", info->signature);
        memset(info->originalSignature, 0, sizeof(info->originalSignature));
        memcpy(info->originalSignature, headStr + ORIGINAL_SIGNATURE_OFFSET, SIGNATURE_LEN);
        printf("info->originalSignature=%s\r\n", info->originalSignature);
        dataOffset = headSize + 5; // 4 byte uint32 and 1 byte string '\0' end.
    } while (0);
    if (headStr != NULL) {
        vPortFree(headStr);
    }
    f_close(&fp);
    return dataOffset;
}

static int32_t CheckOtaFile(OtaFileInfo_t *info, const char *filePath, uint32_t *pHeadSize)
{
    FIL fp;
    int32_t ret;
    uint32_t fileSize, readSize, i, headSize, j;
    int32_t bRet = SUCCESS_CODE;
    sha256_context ctx;

    headSize = GetOtaFileInfo(info, filePath);
    if (headSize == 0 || headSize >= FILE_UNIT_SIZE) {
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
    uint32_t fileSize, readSize, cmpsdSize, decmpsdSize;
    uint32_t decmpsdRet;
    qlz_state_decompress qlzState = {0};
    VersionInfo_t nowVersion = {0};
    VersionInfo_t fileVersion = {0};
    VersionInfo_t otpVersion = {0};
    bool hasOtpVersion;
    bool fileMajorMinorHigherThanOtp = true;
    int32_t cmpFileNow;

    g_shouldWriteOtpVersionHistory = false;
    ret = f_open(&fp, filePath, FA_OPEN_EXISTING | FA_READ);
    if (ret) {
        FatfsError((FRESULT)ret);
        return false;
    }
    fileSize = f_size(&fp);
    f_lseek(&fp, headSize);
    ret = f_read(&fp, g_fileUnit, 16, (UINT *)&readSize);
    if (ret || readSize != 16) {
        FatfsError((FRESULT)ret);
        f_close(&fp);
        return false;
    }
    cmpsdSize = qlz_size_compressed((char *)g_fileUnit);
    decmpsdSize = qlz_size_decompressed((char *)g_fileUnit);
    if (cmpsdSize < 16 || cmpsdSize > sizeof(g_fileUnit) ||
            decmpsdSize == 0 || decmpsdSize > sizeof(g_dataUnit) ||
            cmpsdSize > (fileSize - headSize) || decmpsdSize <= FIXED_SEGMENT_OFFSET) {
        printf("invalid block size: c=%d, d=%d\r\n", cmpsdSize, decmpsdSize);
        f_close(&fp);
        return false;
    }
    printf("cmpsdSize=%d,decmpsdSize=%d\r\n", cmpsdSize, decmpsdSize);
    ret = f_read(&fp, g_fileUnit + 16, cmpsdSize - 16, (UINT *)&readSize);
    if (ret || readSize != cmpsdSize - 16) {
        FatfsError((FRESULT)ret);
        f_close(&fp);
        return false;
    }
    decmpsdRet = qlz_decompress((char *)g_fileUnit, g_dataUnit, &qlzState);
    if (decmpsdRet != decmpsdSize) {
        printf("decompress size mismatch:%d,%d\r\n", decmpsdRet, decmpsdSize);
        f_close(&fp);
        return false;
    }
    GetSoftwareVersion(&nowVersion.major, &nowVersion.minor, &nowVersion.build);
    GetSoftwareVersionFromData(&fileVersion.major, &fileVersion.minor, &fileVersion.build,
                               g_dataUnit + FIXED_SEGMENT_OFFSET, decmpsdSize - FIXED_SEGMENT_OFFSET);
    f_close(&fp);
    printf("now version:%d.%d.%d\n", nowVersion.major, nowVersion.minor, nowVersion.build);
    printf("file version:%d.%d.%d\n", fileVersion.major, fileVersion.minor, fileVersion.build);

    if (!IsValidVersionComponent(nowVersion.major, nowVersion.minor, nowVersion.build) ||
            !IsValidVersionComponent(fileVersion.major, fileVersion.minor, fileVersion.build)) {
        printf("invalid version number, now=%d.%d.%d file=%d.%d.%d\n",
               nowVersion.major, nowVersion.minor, nowVersion.build,
               fileVersion.major, fileVersion.minor, fileVersion.build);
        return false;
    }

    hasOtpVersion = GetLatestOtpHistoryVersion(&otpVersion, NULL, NULL);
    if (hasOtpVersion) {
        int32_t cmpToOtp;
        printf("otp threshold version:%d.%d.%d\n", otpVersion.major, otpVersion.minor, otpVersion.build);
        cmpToOtp = CompareVersion(fileVersion, otpVersion);
        if (cmpToOtp < 0) {
            printf("file version lower than otp threshold\n");
            return false;
        }
        fileMajorMinorHigherThanOtp =
            (fileVersion.major > otpVersion.major) ||
            (fileVersion.major == otpVersion.major && fileVersion.minor > otpVersion.minor);
    } else {
        printf("no otp history version found\n");
        fileMajorMinorHigherThanOtp = true;
    }

    cmpFileNow = CompareVersion(fileVersion, nowVersion);
    if (cmpFileNow > 0) {
        // pass
    } else if (cmpFileNow == 0) {
        if (!(!CheckAppExist() || CalculateCheckSum(true, info->originalHash))) {
            return false;
        }
    } else {
        printf("file version lower than current version\n");
        return false;
    }

    if (cmpFileNow > 0 && fileMajorMinorHigherThanOtp) {
        g_pendingOtpVersion = fileVersion;
        g_shouldWriteOtpVersionHistory = true;
        printf("pending otp history record:%d.%d.%d\n",
               g_pendingOtpVersion.major, g_pendingOtpVersion.minor, g_pendingOtpVersion.build);
    }

    return true;
}

static bool IsValidVersionComponent(uint32_t major, uint32_t minor, uint32_t build)
{
    return (major <= VERSION_COMPONENT_MAX && minor <= VERSION_COMPONENT_MAX && build <= VERSION_COMPONENT_MAX);
}

static uint32_t BuildVersionNumber(uint32_t major, uint32_t minor, uint32_t build)
{
    const uint32_t epoch = 100;

    return (major * epoch * epoch) + (minor * epoch) + build;
}

static int32_t CompareVersion(VersionInfo_t lhs, VersionInfo_t rhs)
{
    uint32_t lhsNumber = BuildVersionNumber(lhs.major, lhs.minor, lhs.build);
    uint32_t rhsNumber = BuildVersionNumber(rhs.major, rhs.minor, rhs.build);

    if (lhsNumber > rhsNumber) {
        return 1;
    }
    if (lhsNumber < rhsNumber) {
        return -1;
    }
    return 0;
}

static uint32_t PackVersionToWord(VersionInfo_t version)
{
    return ((version.major & 0xFFU) << 16) | ((version.minor & 0xFFU) << 8) | (version.build & 0xFFU);
}

static VersionInfo_t UnpackVersionFromWord(uint32_t word)
{
    VersionInfo_t version = {0};

    version.major = (word >> 16) & 0xFFU;
    version.minor = (word >> 8) & 0xFFU;
    version.build = word & 0xFFU;

    return version;
}

bool GetLatestOtpHistoryVersion(VersionInfo_t *version, uint32_t *lastAddr, uint32_t *nextAddr)
{
    uint32_t addr;
    uint32_t latestAddr = 0;
    uint32_t writeAddr = OTP_ADDR_WEB_AUTH_RSA_KEY;
    uint32_t readWord;
    bool hasVersion = false;
    VersionInfo_t latestVersion = {0};

    OTP_PowerOn();
    for (addr = OTP_ADDR_VERISON_HISTROY; addr < OTP_ADDR_WEB_AUTH_RSA_KEY; addr += sizeof(uint32_t)) {
        memcpy(&readWord, (uint8_t *)((uint32_t)addr), sizeof(readWord));
        if (readWord == UINT32_MAX) {
            writeAddr = addr;
            break;
        }

        latestVersion = UnpackVersionFromWord(readWord);
        if (!IsValidVersionComponent(latestVersion.major, latestVersion.minor, latestVersion.build)) {
            printf("invalid otp history version word=%#x,addr=%#x\n", readWord, addr);
            writeAddr = addr;
            break;
        }

        hasVersion = true;
        latestAddr = addr;
    }

    if (hasVersion && version != NULL) {
        *version = latestVersion;
    }
    if (lastAddr != NULL) {
        *lastAddr = latestAddr;
    }
    if (nextAddr != NULL) {
        *nextAddr = writeAddr;
    }

    return hasVersion;
}

static void TryWriteVersionHistoryToOtp(void)
{
    int32_t ret;
    uint32_t writeAddr;
    uint32_t versionWord;
    uint32_t verifyWord = UINT32_MAX;
    uint32_t lastAddr = 0;
    bool hasLatest;
    VersionInfo_t latestVersion = {0};

    if (!g_shouldWriteOtpVersionHistory) {
        return;
    }

    if (!IsValidVersionComponent(g_pendingOtpVersion.major, g_pendingOtpVersion.minor, g_pendingOtpVersion.build)) {
        printf("skip otp history write: invalid pending version=%d.%d.%d\n",
               g_pendingOtpVersion.major, g_pendingOtpVersion.minor, g_pendingOtpVersion.build);
        g_shouldWriteOtpVersionHistory = false;
        return;
    }

    hasLatest = GetLatestOtpHistoryVersion(&latestVersion, &lastAddr, &writeAddr);
    if (hasLatest) {
        printf("latest otp history version:%d.%d.%d at %#x\n",
               latestVersion.major, latestVersion.minor, latestVersion.build, lastAddr);
    } else {
        printf("no valid otp history found\n");
    }
    if (hasLatest && CompareVersion(g_pendingOtpVersion, latestVersion) <= 0) {
        printf("skip otp history write: pending version is not newer than latest otp\n");
        g_shouldWriteOtpVersionHistory = false;
        return;
    }

    if (writeAddr >= OTP_ADDR_WEB_AUTH_RSA_KEY) {
        printf("skip otp history write: otp history area is full\n");
        g_shouldWriteOtpVersionHistory = false;
        return;
    }

    versionWord = PackVersionToWord(g_pendingOtpVersion);
    ret = WriteOtpData(writeAddr, (const uint8_t *)&versionWord, sizeof(versionWord));
    if (ret != 0) {
        printf("write otp history failed,ret=%d,addr=%#x\n", ret, writeAddr);
        g_shouldWriteOtpVersionHistory = false;
        return;
    }

    OTP_PowerOn();
    memcpy(&verifyWord, (uint8_t *)((uint32_t)writeAddr), sizeof(verifyWord));
    if (verifyWord != versionWord) {
        printf("verify otp history failed,addr=%#x,w=%#x,r=%#x\n", writeAddr, versionWord, verifyWord);
        g_shouldWriteOtpVersionHistory = false;
        return;
    }

    printf("otp history recorded at %#x: %d.%d.%d (%#x)\n",
           writeAddr, g_pendingOtpVersion.major, g_pendingOtpVersion.minor, g_pendingOtpVersion.build, versionWord);
    g_shouldWriteOtpVersionHistory = false;
}

static int32_t ReadOtaFileAndCheck(const OtaFileInfo_t *info, const char *filePath, uint32_t headSize, bool write)
{
    FIL fp;
    int32_t ret;
    uint32_t fileSize, readSize, i, offset, cmpsdSize, decmpsdSize, writeAddr, percent;
    uint32_t decmpsdRet;
    sha256_context ctx;
    qlz_state_decompress qlzState = {0};
    static uint32_t lastPercent = 101;
    char percentStr[16];
    uint8_t content_hash[32];

    if (write) {
        DrawRectPic(217, 300, IMGINSTALL_HEIGHT, IMGINSTALL_WIDTH, imgInstall);
        DrawStringOnLcd(135, 422, "            ", 0xFFFF, &openSans_24);
        DrawStringOnLcd(130, 412, "          Installing              ", 0xFFFF, &openSans_24);
        DrawStringOnLcd(215, 620, "               ", 0xFFFF, &openSans_24);
    } else {
        DrawRectPic(217, 300, IMGINSTALL_HEIGHT, IMGINSTALL_WIDTH, imgSecurity);
        DrawStringOnLcd(135, 412, "Upgrade Verification", 0xFFFF, &openSans_24);
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
        uint32_t blockStart = i;
        uint32_t blockRemain;
        ret = f_read(&fp, g_fileUnit, 16, (UINT *)&readSize);
        if (ret || readSize != 16) {
            FatfsError((FRESULT)ret);
            f_close(&fp);
            return ERR_UPDATE_CHECK_FILE_EXIST;
        }
        i += readSize;
        blockRemain = fileSize - blockStart;
        cmpsdSize = qlz_size_compressed((char *)g_fileUnit);
        decmpsdSize = qlz_size_decompressed((char *)g_fileUnit);
        if (cmpsdSize < 16 || cmpsdSize > sizeof(g_fileUnit) ||
                decmpsdSize == 0 || decmpsdSize > sizeof(g_dataUnit) ||
                cmpsdSize > blockRemain || (decmpsdSize % 4096) != 0) {
            printf("invalid block size in update, c=%d,d=%d,remain=%d,start=%d\r\n",
                   cmpsdSize, decmpsdSize, blockRemain, blockStart);
            f_close(&fp);
            return ERR_UPDATE_CHECK_CRC_FAILED;
        }
        if (write && (writeAddr + decmpsdSize > APP_SIGNATURE_ADDR)) {
            printf("write range overflow, addr=%#x,size=%d\r\n", writeAddr, decmpsdSize);
            f_close(&fp);
            return ERR_UPDATE_CHECK_CRC_FAILED;
        }
        ret = f_read(&fp, g_fileUnit + 16, cmpsdSize - 16, (UINT *)&readSize);
        if (ret || readSize != cmpsdSize - 16) {
            FatfsError((FRESULT)ret);
            f_close(&fp);
            return ERR_UPDATE_CHECK_FILE_EXIST;
        }
        decmpsdRet = qlz_decompress((char *)g_fileUnit, g_dataUnit, &qlzState);
        if (decmpsdRet != decmpsdSize) {
            printf("decompress size mismatch:%d,%d\r\n", decmpsdRet, decmpsdSize);
            f_close(&fp);
            return ERR_UPDATE_CHECK_CRC_FAILED;
        }
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
#if (SIGNATURE_ENABLE == 1)
    uint8_t publickey[65] = {0};
    GetUpdatePubKey(publickey);
    if (memcmp(content_hash, info->originalHash, 32) == 0) {
        if (verify_frimware_signature((char *)info->originalSignature, content_hash, publickey)) {
            printf("check signature ok\r\n");
            char *signature = pvPortMalloc(4096);
            if (signature == NULL) {
                f_close(&fp);
                return ERR_UPDATE_CHECK_SIGNATURE_FAILED;
            }
            memset(signature, 0, 4096);
            memcpy(signature, info->originalSignature, sizeof(info->originalSignature));
            if (write) {
                QspiFlashEraseAndWrite(APP_SIGNATURE_ADDR, (const uint8_t *)signature, 4096);
            }
            printf("signature:%s\r\n", signature);
            memset(signature, 0, 4096);
            vPortFree(signature);
            f_close(&fp);
            return SUCCESS_CODE;
        } else {
            printf("check signature failed\r\n");
            f_close(&fp);
            return ERR_UPDATE_CHECK_SIGNATURE_FAILED;
        }
    } else {
        printf("update failed\r\n");
        char *signature = pvPortMalloc(4096);
        if (signature == NULL) {
            f_close(&fp);
            return ERR_UPDATE_CHECK_CRC_FAILED;
        }
        memset(signature, 0, 4096);
        for (int i = 0; i < 256; i++) {
            signature[i] = i;
        }
        if (write) {
            QspiFlashEraseAndWrite(APP_SIGNATURE_ADDR, (const uint8_t *)signature, 4096);
        }
        memset(signature, 0, 4096);
        vPortFree(signature);
        f_close(&fp);
        return ERR_UPDATE_CHECK_CRC_FAILED;
    }
#else
    char *signature = pvPortMalloc(4096);
    if (signature == NULL) {
        f_close(&fp);
        return ERR_UPDATE_CHECK_SIGNATURE_FAILED;
    }
    memset(signature, 0, 4096);
    memcpy(signature, info->originalSignature, sizeof(info->originalSignature));
    QspiFlashEraseAndWrite(APP_SIGNATURE_ADDR, (const uint8_t *)signature, 4096);
    printf("signature:%s\r\n", signature);
    memset(signature, 0, 4096);
    vPortFree(signature);
#endif
    f_close(&fp);
    return SUCCESS_CODE;
}

void QspiFlashWriteFF(uint32_t addr, uint32_t size)
{
    memset(g_fileUnit, 0xFF, sizeof(g_fileUnit));
    QspiFlashEraseAndWrite(addr, g_fileUnit, size);
}

static int32_t UpdateFromOtaFile(const OtaFileInfo_t *info, const char *filePath, uint32_t headSize)
{
    int32_t ret = ReadOtaFileAndCheck(info, filePath, headSize, false);
    if (ret != SUCCESS_CODE) {
        return ret;
    }
    return ReadOtaFileAndCheck(info, filePath, headSize, true);
}

#if (SIGNATURE_ENABLE == 1)
static void GetUpdatePubKey(uint8_t *pubKey)
{
    uint8_t data[UPDATE_PUB_KEY_LEN];
    int32_t addr;

    pubKey[0] = 4;
    OTP_PowerOn();
    for (addr = OTP_ADDR_UPDATE_PUB_KEY + 1024 - UPDATE_PUB_KEY_LEN; addr >= (int32_t)OTP_ADDR_UPDATE_PUB_KEY; addr -= UPDATE_PUB_KEY_LEN) {
        memcpy(data, (uint8_t *)((uint32_t)addr), UPDATE_PUB_KEY_LEN);
        // PrintArray("read pub key", data, UPDATE_PUB_KEY_LEN);
        if (CheckAllFF(data, UPDATE_PUB_KEY_LEN) == false) {
            if (CheckEntropy(data, UPDATE_PUB_KEY_LEN)) {
                // Found
                printf("found,addr=0x%X\n", (uint32_t)addr);
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

    memset(signature, 0, 256 + 1);
    memcpy(signature, (uint32_t *)(APP_SIGNATURE_ADDR), 256);
    signature[256] = '\0';
    if (CheckAllFF((const uint8_t *)signature, 256)) {
        vPortFree(signature);
        return false;
    }
    bool isOK = verify_frimware_signature(signature, (uint8_t *)hash, publickey);
    vPortFree(signature);

    return isOK;
}
#endif

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

static void AesEncryptBuffer(uint8_t *cipher, uint32_t sz, uint8_t *plain)
{
    uint8_t key128[16] = {0};
    uint8_t iv[16] = {0};

    OTP_PowerOn();
    memcpy(key128, (uint32_t *)(0x40009128), 16);
    memcpy(iv, (uint32_t *)(0x40009138), 16);

    AES128_CBC_ctx ctx;
    AES128_CBC_init(&ctx, key128, iv);
    AES128_CBC_encrypt(&ctx, sz / 16, cipher, plain);
    memset(key128, 0, 16);
    memset(iv, 0, 16);
}

void AesDecryptBuffer(uint8_t *plain, uint32_t sz, uint8_t *cipher)
{
    AES128_CBC_ctx ctx;
    uint8_t key128[16] = {0};
    uint8_t iv[16] = {0};

    OTP_PowerOn();
    memcpy(key128, (uint32_t *)(0x40009128), 16);
    memcpy(iv, (uint32_t *)(0x40009138), 16);
    AES128_CBC_init(&ctx, key128, iv);
    AES128_CBC_decrypt(&ctx, sz / 16, plain, cipher);
    memset(key128, 0, 16);
    memset(iv, 0, 16);
}

bool GetFactoryResult(void)
{
    uint32_t data;
    OTP_PowerOn();
    memcpy(&data, (uint32_t *)OTA_ADDR_FACTORY_BASE, 4);
    if (data != 0xFFFFFFFF) {
        printf("data=%#x........\n", data);
        printf("factory pass\n");
        return true;
    } else {
        printf("factory not pass\n");
        return false;
    }
}

void InitBootParam(void)
{
    BootParam_t bootParam;
    Gd25FlashReadBuffer(BOOT_SECURE_PARAM_FLAG, (uint8_t *)&bootParam, sizeof(bootParam));
    if ((CheckAllFF((uint8_t *)&bootParam, sizeof(bootParam)) || CheckAllZero((uint8_t *)&bootParam, sizeof(bootParam))) &&
            !GetFactoryResult()) {
        printf("bootParam is all FF or all 0\n");
        memcpy(g_bootParam.bootCheckFlag, g_integrityFlag, sizeof(g_bootParam.bootCheckFlag));
        memcpy(g_bootParam.recoveryModeSwitch, g_integrityFlag, sizeof(g_bootParam.recoveryModeSwitch));
        return;
    }
    AesDecryptBuffer((uint8_t *)&g_bootParam, sizeof(g_bootParam), (uint8_t *)&bootParam);
}

bool GetBootSecureCheckFlag(void)
{
    return (memcmp(g_bootParam.bootCheckFlag, g_integrityFlag, sizeof(g_integrityFlag)) == 0);
}

bool GetRecoveryModeFlag(void)
{
    PrintArray("check bootParam.recoveryModeSwitch", g_bootParam.recoveryModeSwitch, sizeof(g_bootParam.recoveryModeSwitch));
    return (memcmp(g_bootParam.recoveryModeSwitch, g_integrityFlag, sizeof(g_integrityFlag)) == 0 ||
            memcmp(g_bootParam.recoveryModeSwitch, g_recoveryModeFlag, sizeof(g_recoveryModeFlag)) == 0);
}

void ResetBootParam(void)
{
    memcpy(g_bootParam.bootCheckFlag, g_integrityFlag, sizeof(g_bootParam.bootCheckFlag));
    memcpy(g_bootParam.recoveryModeSwitch, g_recoveryModeFlag, sizeof(g_bootParam.recoveryModeSwitch));
    const uint8_t MAX_RETRY = 3;
    uint8_t retryCount = 0;
    int32_t result;
    do {
        result = SaveBootParam();
        if (result == SUCCESS_CODE) {
            printf("SaveBootParam success on attempt %d\n", retryCount + 1);
            break;
        }
        retryCount++;
        printf("SaveBootParam failed on attempt %d, error code: %d\n", retryCount, result);
    } while (retryCount < MAX_RETRY);

    if (result != SUCCESS_CODE) {
        printf("SaveBootParam failed after %d attempts\n", MAX_RETRY);
    }
}

int SaveBootParam(void)
{
    int ret = SUCCESS_CODE;
    uint8_t cipher[sizeof(g_bootParam)] = {0};
    uint8_t plain[sizeof(g_bootParam)] = {0};
    uint8_t verifyBuffer[sizeof(g_bootParam)] = {0};

    if (sizeof(g_bootParam) > BOOT_SECURE_PARAM_FLAG_SIZE) {
        return ERR_GD25_BAD_PARAM;
    }

    memcpy(plain, &g_bootParam, sizeof(g_bootParam));

    AesEncryptBuffer(cipher, sizeof(cipher), plain);

    if (Gd25FlashSectorErase(BOOT_SECURE_PARAM_FLAG) != SUCCESS_CODE) {
        ret = ERR_GD25_BAD_PARAM;
        goto cleanup;
    }

    Gd25FlashWriteBuffer(BOOT_SECURE_PARAM_FLAG, cipher, sizeof(cipher));

    Gd25FlashReadBuffer(BOOT_SECURE_PARAM_FLAG, verifyBuffer, sizeof(verifyBuffer));

    if (memcmp(cipher, verifyBuffer, sizeof(cipher)) != 0) {
        ret = ERR_GD25_BAD_PARAM;
    }

cleanup:
    memset(plain, 0, sizeof(plain));
    memset(cipher, 0, sizeof(cipher));
    memset(verifyBuffer, 0, sizeof(verifyBuffer));

    return ret;
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
        DrawRectPic(204, 315, IMGSECURITY_HEIGHT, IMGSECURITY_WIDTH, imgSecurity);
        DrawStringOnLcd(120, 407, "Firmware Verification", 0xFFFF, &openSans_24);
        DrawStringOnLcd(100, 457, "Checking firmware, please wait", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
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

void JumpToApp(void)
{
    typedef int (*jumpApp)(void);
    volatile uint32_t *ptr = (uint32_t *)APP_ADDR;
    jumpApp app;
    uint32_t msp;
    uint32_t resetHandler;

    msp = ptr[0];
    resetHandler = ptr[1];
    if (!IsValidAppVector(msp, resetHandler)) {
        printf("invalid app vector,msp=%#x,reset=%#x\r\n", msp, resetHandler);
        return;
    }

    app = (jumpApp)resetHandler;
    __disable_irq();
    __set_MSP(msp);
    app();
}

static void HandleFirmwareVerificationFailed(void)
{
    ReducedGlInit();
    DrawRectPic(204, 155, IMGWARN_HEIGHT, IMGWARN_WIDTH, imgWarn);
    DrawStringOnLcd(135, 259, "Verification Failure", 0xFFFF, &openSans_24);
    DrawStringOnLcd(60, 311, "Please reboot the device and try again. If", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
    DrawStringOnLcd(65, 341, "the issue persists, the firmware may be", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
    DrawStringOnLcd(50, 371, "incomplete or corrupted. Please wipe the", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
    DrawStringOnLcd(50, 401, "device and reinstall the firmware from the", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
    DrawStringOnLcd(165, 431, "official website.", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
    DrawStringOnLcd(55, 461, "Alternatively, you can access the system", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
    DrawStringOnLcd(55, 491, "now to transfer your assets to a secure", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);
    DrawStringOnLcd(190, 521, "address", _COLOR_MAKE(0x66, 0x66, 0x66), &openSans_20);

    CreateRadiusButton(36 + 32, 611, 408 - 64, 64, _COLOR_MAKE(0xF5, 0x58, 0x31),
                       _COLOR_MAKE(0xF5, 0x58, 0x31), "Wipe Device", WipeDeviceCallbackFunc);
    CreateRadiusButton(36 + 32, 698, 408 - 64, 64, _COLOR_MAKE(0x33, 0x33, 0x33),
                       _COLOR_MAKE(0x33, 0x33, 0x33), "Enter System", JumpToApp);
    while (1) {
        ReducedGlHandler();
    }
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

static uint32_t BytesToUint32BE(char *bytes)
{
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | (uint32_t)bytes[3];
}

static bool IsAddressInRange(uint32_t addr, uint32_t start, uint32_t end)
{
    return (addr >= start) && (addr < end);
}

static bool IsValidAppVector(uint32_t msp, uint32_t resetHandler)
{
    uint32_t resetAddr = resetHandler & (~1U);
    if ((msp == 0xFFFFFFFF) || (resetHandler == 0xFFFFFFFF)) {
        return false;
    }
    if ((resetHandler & 1U) == 0) {
        return false;
    }
    if ((msp & 0x3) != 0) {
        return false;
    }
    if (!IsAddressInRange(msp, SRAM_START_ADDR, SRAM_END_ADDR)) {
        return false;
    }
    if (!IsAddressInRange(resetAddr, APP_FLASH_START_ADDR, APP_FLASH_END_ADDR)) {
        return false;
    }
    return true;
}

static bool IsHexChar(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

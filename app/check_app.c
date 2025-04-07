#include "check_app.h"
#include "string.h"
#include "stdio.h"
#include "firmware_update.h"
#include "log_print.h"
#include "hal_lcd.h"
#include "user_delay.h"
#include "draw_on_lcd.h"
#include "drv_power.h"
#include "drv_lcd_bright.h"
#include "user_utils.h"

#define CHECK_UNIT              256
#define CHECK_SIZE              4096

#define APP_VERSION_ADDR        0x01082000
#define APP_VERSION_HEAD        "Firmware v"

//static bool CheckAllFF(const uint8_t *data, uint32_t length);

LV_FONT_DECLARE(openSans_24);

/// @brief Check the app flash zone data that if there is a firmware already installed.
/// @return true-there is a firmware already installed.
bool CheckApp(void)
{
    uint8_t read[4096];
    uint32_t major, minor, build;
    memcpy(read, (void *)APP_VERSION_ADDR, 4096);
    return GetSoftwareVersionFormData(&major, &minor, &build, read, 4096) == 0;
}

bool CheckAppExist(void)
{
    uint8_t read[4096];
    memcpy(read, (void *)APP_ADDR, 4096);
    return !CheckAllFF(read, 4096);
}

#define PILLAR_TEST_APP_MAJOR           (0)
#define PILLAR_TEST_APP_MINOR           (0)
#define PILLAR_TEST_APP_BUILD           (1)
bool CheckAppFactory(void)
{
    uint32_t nowMajor, nowMinor, nowBuild;
    GetSoftwareVersion(&nowMajor, &nowMinor, &nowBuild);
    printf("now version:%d.%d.%d\n", nowMajor, nowMinor, nowBuild);
    if (PILLAR_TEST_APP_MAJOR == nowMajor && PILLAR_TEST_APP_MINOR == nowMinor && PILLAR_TEST_APP_BUILD == nowBuild) {
        return true;
    }
    return false;
}

void GetSoftwareVersion(uint32_t *major, uint32_t *minor, uint32_t *build)
{
    uint8_t read[4096];

    memcpy(read, (void *)APP_VERSION_ADDR, 4096);
    GetSoftwareVersionFormData(major, minor, build, read, 4096);
}


int32_t GetSoftwareVersionFormData(uint32_t *major, uint32_t *minor, uint32_t *build, const uint8_t *data, uint32_t dataLen)
{
    uint32_t versionInfoOffset = UINT32_MAX, i, headLen;
    char *versionStr, read[64];
    int32_t ret;
    bool succ = false;

    headLen = strlen(APP_VERSION_HEAD);
    for (i = 0; i < dataLen - headLen - 32; i++) {
        if (data[i] == 'F') {
            if (strncmp((char *)&data[i], APP_VERSION_HEAD, headLen) == 0) {
                versionInfoOffset = i;
                break;
            }
        }
    }
    do {
        if (versionInfoOffset == UINT32_MAX) {
            printf("version string not found in fixed segment\n");
            break;
        }
        memcpy(read, &data[versionInfoOffset], 64);
        read[31] = '\0';
        if (strncmp(read, APP_VERSION_HEAD, headLen) != 0) {
            break;
        }
        versionStr = read + headLen;
        //printf("versionStr=%s\n", versionStr);
        ret = sscanf(versionStr, "%d.%d.%d", major, minor, build);
        if (ret != 3) {
            break;
        }
        succ = true;
    } while (0);
    if (succ == false) {
        *major = 0;
        *minor = 0;
        *build = 0;
    }
    return succ ? 0 : -1;
}


//static bool CheckAllFF(const uint8_t *data, uint32_t length)
//{
//    for (uint32_t i = 0; i < length; i++) {
//        if (data[i] != 0xFF) {
//            return false;
//        }
//    }
//    return true;
//}


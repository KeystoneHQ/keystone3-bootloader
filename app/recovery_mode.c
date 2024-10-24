#include "recovery_mode.h"
#include "string.h"
#include "stdio.h"
#include "mhscpu.h"
#include "log_print.h"
#include "hal_lcd.h"
#include "user_delay.h"
#include "draw_on_lcd.h"
#include "drv_power.h"
#include "drv_lcd_bright.h"
#include "drv_usb.h"
#include "reduced_gl.h"
#include "drv_aw32001.h"
#include "check_app.h"
#include "drv_qspi_flash.h"
#include "drv_gd25qxx.h"
#include "firmware_update.h"
#include "drv_ds28s60.h"
#include "user_fatfs.h"


LV_FONT_DECLARE(openSans_20);
LV_FONT_DECLARE(openSans_24);

#define BOOTLOADER_VERSION              "v0.2.0"

#define BUTTON_PORT                     GPIOE
#define BUTTON_PIN                      GPIO_Pin_14

#define USB_DET_PORT                    GPIOF
#define USB_DET_PIN                     GPIO_Pin_15

#define RECOVERY_MODE_WAIT_TICK         3000

#define MAX_QSPI_FLASH_SIZE             (16 * 1024 * 1024 - 0x81000)
#define MAX_SPI_FLASH_SIZE              (16 * 1024 * 1024)

#define APP_VERSION_ADDR                0x01082000

static void RecoveryModeMainMenu(void);
static void PowerOffMenu(void);
static void WipeDeviceMenu(void);

static void RebootCallbackFunc(void);
static void PowerOffCallbackFunc(void);
static void WipeDeviceCallbackFunc(void);

static void RecoveryHandler(void);

static bool g_availableUpdateFile = false;
static bool g_refreshFileStatus = 0;

bool OptionToRecoveryMode(void)
{
    uint32_t tick;
    GPIO_InitTypeDef gpioInit = {0};

    gpioInit.GPIO_Mode = GPIO_Mode_IPU;
    gpioInit.GPIO_Pin = BUTTON_PIN;
    gpioInit.GPIO_Remap = GPIO_Remap_1;
    GPIO_Init(BUTTON_PORT, &gpioInit);

    gpioInit.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpioInit.GPIO_Pin = USB_DET_PIN;
    gpioInit.GPIO_Remap = GPIO_Remap_1;
    GPIO_Init(USB_DET_PORT, &gpioInit);

    tick = 0;
    while (1) {
        if (GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN) == Bit_RESET && GPIO_ReadInputDataBit(USB_DET_PORT, USB_DET_PIN) == Bit_RESET) {
            //Press button & Insert USB.
            tick++;
            if (tick > RECOVERY_MODE_WAIT_TICK) {
                return true;
            }
            UserDelay(1);
        } else {
            return false;
        }
    }
}


void RecoveryMode(void)
{
    printf("goto recovery mode\n");
    SetLcdBright(0);
    OpenPower(POWER_TYPE_VCC33);
    LcdCheck();
    LcdInit();
    UserDelay(100);
    SetLcdBright(70);
    UsbInit();
    ReducedGlInit();
    RecoveryModeMainMenu();

    while (1) {
        ReducedGlHandler();
        RecoveryHandler();
    }
}


void RefreshFileStatus(void)
{
    g_refreshFileStatus = true;
}


static void RecoveryModeMainMenu(void)
{
    char showString[64];
    uint32_t major, minor, build;
    DeleteAllWidgets();
    CreateLabel(150, 120, 0xFFFF, "Recovery  Mode");
    CreateLabel(200, 160, 0xFFFF, BOOTLOADER_VERSION);
#ifndef __GNUC__
    CreateLabel(140, 200, _COLOR_MAKE(255, 0, 0), "*Developer Mode*");
#endif
    CreateButton(100, 300, 280, 60, _COLOR_MAKE(0, 0, 255), _COLOR_MAKE(0, 0, 150), "Reboot", RebootCallbackFunc);
    CreateButton(100, 400, 280, 60, _COLOR_MAKE(0, 0, 255), _COLOR_MAKE(0, 0, 150), "Power Off", PowerOffMenu);
    CreateButton(100, 500, 280, 60, _COLOR_MAKE(0, 0, 255), _COLOR_MAKE(0, 0, 150), "Wipe Device", WipeDeviceMenu);
    if (CheckApp() == false) {
        CreateLabel(200, 650, 0xFFFF, "NO APP");
    } else {
        GetSoftwareVersion(&major, &minor, &build);
        sprintf(showString, "APP version %d.%d.%d", major, minor, build);
        CreateLabel(140, 620, 0xFFFF, showString);
    }
    if (g_availableUpdateFile) {
        CreateLabel(30, 680, _COLOR_MAKE(0, 255, 0), "There is a available update file, reboot");
        CreateLabel(30, 720, _COLOR_MAKE(0, 255, 0), "the device to update firmware.");
    }
}


static void PowerOffMenu(void)
{
    DeleteAllWidgets();
    CreateLabel(30, 120, 0xFFFF, "In order to shut down, please make");
    CreateLabel(30, 170, 0xFFFF, "sure to unplug the USB cable.");
    CreateButton(150, 400, 180, 60, _COLOR_MAKE(255, 0, 0), _COLOR_MAKE(150, 0, 0), "OK", PowerOffCallbackFunc);
    CreateButton(150, 500, 180, 60, _COLOR_MAKE(60, 60, 60), _COLOR_MAKE(30, 30, 30), "Cancel", RecoveryModeMainMenu);
}


static void WipeDeviceMenu(void)
{
    DeleteAllWidgets();
    CreateLabel(50, 120, 0xFFFF, "Are you sure to wipe device?");
    CreateButton(150, 400, 180, 60, _COLOR_MAKE(255, 0, 0), _COLOR_MAKE(150, 0, 0), "YES", WipeDeviceCallbackFunc);
    CreateButton(150, 500, 180, 60, _COLOR_MAKE(60, 60, 60), _COLOR_MAKE(30, 30, 30), "Cancel", RecoveryModeMainMenu);
}


static void RebootCallbackFunc(void)
{
    printf("RebootCallbackFunc\n");
    NVIC_SystemReset();
}


static void PowerOffCallbackFunc(void)
{
    Aw32001Init();
    Aw32001PowerOff();
    CreateLabel(40, 650, 0xFFFF, "Turning the power off now...");
    printf("PowerOffCallbackFunc\n");
}


static void WipeDeviceCallbackFunc(void)
{
    uint32_t percent, lastPercent, addr;
    char percentStr[16];
    uint8_t pageData[32], page;

    UsbDeInit();
    DeleteAllWidgets();
    //CreateLabel(50, 120, 0xFFFF, "Wiping device now...");
    ReducedGlHandler();
    DrawStringOnLcd(50, 120, "Wiping device now...", 0xFFFF, &openSans_20);
    DS28S60_Init();

    DrawStringOnLcd(50, 360, "Erasing SE...", 0xFFFF, &openSans_20);
    printf("Erasing SE...\n");
    DrawStringOnLcd(215, 620, "            ", 0xFFFF, &openSans_24);
    memset(pageData, 0, sizeof(pageData));
    for (page = 0; page <= MAX_USER_PAGE; page++) {
        if (page == 82) {
            continue;
        }
        percent = page * 100 / MAX_USER_PAGE;
        if (percent != lastPercent) {
            lastPercent = percent;
            //printf("percent=%d\n", percent);
            sprintf(percentStr, "%d%%", percent);
            DrawStringOnLcd(215, 620, percentStr, 0xFFFF, &openSans_24);
            DrawProgressBarOnLcd(80, 594, 320, 9, percent, 0x21F4);
        }
        DS28S60_HmacEncryptWrite(pageData, page);
    }

    lastPercent = 101;
    DrawStringOnLcd(50, 200, "Erasing QSPI FLASH...", 0xFFFF, &openSans_20);
    printf("Erasing QSPI FLASH...\n");
    DrawStringOnLcd(215, 620, "            ", 0xFFFF, &openSans_24);
    for (addr = APP_ADDR; addr < APP_ADDR + MAX_QSPI_FLASH_SIZE; addr += 4096) {
        percent = (addr - APP_ADDR) * 100 / MAX_QSPI_FLASH_SIZE;
        if (percent != lastPercent) {
            lastPercent = percent;
            sprintf(percentStr, "%d%%", percent);
            DrawStringOnLcd(215, 620, percentStr, 0xFFFF, &openSans_24);
            DrawProgressBarOnLcd(80, 594, 320, 9, percent, 0x21F4);
        }
        if (addr == APP_VERSION_ADDR && !CheckAppFactory()) {
            continue;
        }
        QspiFlashErase(addr);
    }

    DrawStringOnLcd(50, 280, "Erasing SPI FLASH...", 0xFFFF, &openSans_20);
    printf("Erasing SPI FLASH...\n");
    DrawStringOnLcd(215, 620, "            ", 0xFFFF, &openSans_24);
    percent = 0;
    sprintf(percentStr, "%d%%", percent);
    DrawStringOnLcd(215, 620, percentStr, 0xFFFF, &openSans_24);
    DrawProgressBarOnLcd(80, 594, 320, 9, percent, 0x21F4);
    Gd25FlashChipErase();
    percent = 100;
    sprintf(percentStr, "%d%%", percent);
    DrawStringOnLcd(215, 620, percentStr, 0xFFFF, &openSans_24);
    DrawProgressBarOnLcd(80, 594, 320, 9, percent, 0x21F4);

    UserDelay(200);
    NVIC_SystemReset();
}


static void RecoveryHandler(void)
{
    if (g_refreshFileStatus) {
        g_refreshFileStatus = false;
        FIL fp;
        uint32_t fileSize;
        FRESULT res;
        MountUsbFatfs();
        g_availableUpdateFile = false;
        res = f_open(&fp, "1:pillar.bin", FA_OPEN_EXISTING | FA_READ);
        if (res) {
            printf("open error\r\n");
        } else {
            fileSize = f_size(&fp);
            printf("file size=%d\r\n", fileSize);
            if (fileSize > 0) {
                g_availableUpdateFile = true;
            }
        }
        res = f_open(&fp, "1:keystone3.bin", FA_OPEN_EXISTING | FA_READ);
        if (res) {
            printf("open error\r\n");
        } else {
            fileSize = f_size(&fp);
            printf("file size=%d\r\n", fileSize);
            if (fileSize > 0) {
                g_availableUpdateFile = true;
            }
        }
        RecoveryModeMainMenu();
    }
}

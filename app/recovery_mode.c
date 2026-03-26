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
#include "reduced_gl.h"
#include "drv_aw32001.h"
#include "check_app.h"
#include "drv_qspi_flash.h"
#include "drv_gd25qxx.h"
#include "firmware_update.h"
#include "drv_ds28s60.h"
#include "user_fatfs.h"
#include "imgDamaged.h"


LV_FONT_DECLARE(openSans_20);
LV_FONT_DECLARE(openSans_24);

#define BOOTLOADER_VERSION              "v0.3.0"
const char g_softwareVersionString[] __attribute__((section(".fixSection"))) = "Boot v0.3.0";

#define BUTTON_PORT                     GPIOE
#define BUTTON_PIN                      GPIO_Pin_14

#define USB_DET_PORT                    GPIOF
#define USB_DET_PIN                     GPIO_Pin_15

#define RECOVERY_MODE_WAIT_TICK         3000

#define MAX_QSPI_FLASH_SIZE             (16 * 1024 * 1024 - 0x81000)
#define MAX_SPI_FLASH_SIZE              (16 * 1024 * 1024)

#define APP_VERSION_ADDR                0x01082000

#define ORANGE_RED_COLOR                _COLOR_MAKE(0xF5, 0x56, 0x31)
#define GRAY_COLOR                      _COLOR_MAKE(0x3C, 0x3C, 0x3C)
#define DEEP_GRAY_COLOR                 _COLOR_MAKE(0x66, 0x66, 0x66)

static void RecoveryModeMainMenu(void);
static void PowerOffMenu(void);
static void WipeDeviceMenu(void);
static void CopyUpdateFirmwareMenu(void);

static void RebootCallbackFunc(void);
static void PowerOffCallbackFunc(void);
void WipeDeviceCallbackFunc(void);

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
    CreateRadiusButton(100, 250, 280, 60, _COLOR_MAKE(0, 0, 255), _COLOR_MAKE(0, 0, 150), "Reboot", RebootCallbackFunc);
    CreateRadiusButton(100, 350, 280, 60, _COLOR_MAKE(0, 0, 255), _COLOR_MAKE(0, 0, 150), "Power Off", PowerOffMenu);
    CreateRadiusButton(100, 450, 280, 60, _COLOR_MAKE(0, 0, 255), _COLOR_MAKE(0, 0, 150), "Update (SD Card)", CopyUpdateFirmwareMenu);
    CreateRadiusButton(100, 550, 280, 60, _COLOR_MAKE(0, 0, 255), _COLOR_MAKE(0, 0, 150), "Wipe Device", WipeDeviceMenu);

    if (CheckApp() == false) {
        CreateLabel(200, 650, 0xFFFF, "NO APP");
    } else {
        if (CheckAppExist() == false) {
            CreateLabel(200, 660, 0xFFFF, "NO APP");
            GetSoftwareVersion(&major, &minor, &build);
            sprintf(showString, "Last version %d.%d.%d", major, minor, build);
            CreateLabel(140, 610, 0xFFFF, showString);
        } else {
            GetSoftwareVersion(&major, &minor, &build);
            sprintf(showString, "APP version %d.%d.%d", major, minor, build);
            CreateLabel(140, 620, 0xFFFF, showString);
        }
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
    CreateRadiusButton(133, 400, 213, 56, _COLOR_MAKE(255, 0, 0), _COLOR_MAKE(150, 0, 0), "OK", PowerOffCallbackFunc);
    CreateRadiusButton(133, 500, 213, 56, _COLOR_MAKE(60, 60, 60), _COLOR_MAKE(30, 30, 30), "Cancel", RecoveryModeMainMenu);
}

static void CopyUpdateFirmwareMenu(void)
{
    DeleteAllWidgets();
    Error_Code errCode = CopyBin2Flash(true);
    if (errCode == SUCCESS_CODE) {
        FirmwareUpdate(UPDATE_KEYSTONE3_PATH);
        NVIC_SystemReset();
    } else if (errCode == ERR_UPDATE_CHECK_FILE_EXIST) {
        DeleteAllWidgets();
        DrawRectPic(204, 176, IMGDAMAGED_HEIGHT, IMGDAMAGED_WIDTH, imgDamaged);
        DrawStringOnLcd(120, 280, "Firmware Not Detected", 0xFFFF, &openSans_24);
        DrawStringOnLcd(50, 328, "Please ensure that your MicroSD card is ", DEEP_GRAY_COLOR, &openSans_20);
        DrawStringOnLcd(28, 368, "formatted in FAT32 and contains the firmware", DEEP_GRAY_COLOR, &openSans_20);
        DrawStringOnLcd(158, 408, "\"keystone3.bin\".", DEEP_GRAY_COLOR, &openSans_20);
        CreateRadiusButton(133, 495, 213, 56, ORANGE_RED_COLOR, _COLOR_MAKE(150, 0, 0), "Retry", CopyUpdateFirmwareMenu);
        CreateRadiusButton(133, 602, 213, 56, GRAY_COLOR, _COLOR_MAKE(30, 30, 30), "Cancel", RecoveryModeMainMenu);
    } else if (errCode == ERR_UPDATE_MOUNT_FAILED) {
        DeleteAllWidgets();
        DrawRectPic(204, 176, IMGDAMAGED_HEIGHT, IMGDAMAGED_WIDTH, imgDamaged);
        DrawStringOnLcd(85, 280, "MicroSD Card Not Detected", 0xFFFF, &openSans_24);
        DrawStringOnLcd(36, 328, "Please ensure that you have properly inserted", DEEP_GRAY_COLOR, &openSans_20);
        DrawStringOnLcd(158, 368, "the MicroSD Card.", DEEP_GRAY_COLOR, &openSans_20);
        CreateRadiusButton(133, 495, 213, 56, ORANGE_RED_COLOR, _COLOR_MAKE(150, 0, 0), "Retry", CopyUpdateFirmwareMenu);
        CreateRadiusButton(133, 602, 213, 56, GRAY_COLOR, _COLOR_MAKE(30, 30, 30), "Cancel", RecoveryModeMainMenu);
    } else {
        RecoveryModeMainMenu();
    }
}


static void WipeDeviceMenu(void)
{
    DeleteAllWidgets();
    DrawStringOnLcd(168, 176, "Wipe Device", 0xFFFF, &openSans_24);
    DrawStringOnLcd(31, 224, "By proceeding,all data on this device,including", DEEP_GRAY_COLOR, &openSans_20);
    DrawStringOnLcd(41, 254, "all your wallets,will be permanently deleted.", DEEP_GRAY_COLOR, &openSans_20);
    DrawStringOnLcd(120, 284, "Are you sure to wipe device?", DEEP_GRAY_COLOR, &openSans_20);
    CreateRadiusButton(133, 495, 213, 56, _COLOR_MAKE(255, 0, 0), _COLOR_MAKE(150, 0, 0), "Yes", WipeDeviceCallbackFunc);
    CreateRadiusButton(133, 602, 213, 56, _COLOR_MAKE(60, 60, 60), _COLOR_MAKE(30, 30, 30), "Cancel", RecoveryModeMainMenu);
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

void WipeDeviceCallbackFunc(void)
{
    uint32_t percent = 0, lastPercent = 0, addr;
    char percentStr[16];
    uint8_t pageData[32], page;

    DeleteAllWidgets();
    ReducedGlHandler();
    DrawStringOnLcd(50, 120, "Wiping device now...", 0xFFFF, &openSans_20);
    DS28S60_Init();

    DrawStringOnLcd(50, 200, "Erasing SE...", 0xFFFF, &openSans_20);
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
            sprintf(percentStr, "%d%%", percent);
            DrawStringOnLcd(215, 620, percentStr, 0xFFFF, &openSans_24);
            DrawProgressBarOnLcd(80, 594, 320, 9, percent, 0x21F4);
        }
        DS28S60_HmacEncryptWrite(pageData, page);
    }

    lastPercent = 101;
    DrawStringOnLcd(50, 280, "Erasing QSPI FLASH...", 0xFFFF, &openSans_20);
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
        if (addr == APP_VERSION_ADDR) {
            continue;
        }
        if (addr == APP_ADDR) {
            QspiFlashWriteFF(APP_ADDR, 4096);
            continue;
        }
        QspiFlashErase(addr);
    }

    DrawStringOnLcd(50, 360, "Erasing SPI FLASH...", 0xFFFF, &openSans_20);
    printf("Erasing SPI FLASH...\n");
    DrawStringOnLcd(215, 620, "            ", 0xFFFF, &openSans_24);
    percent = 0;
    sprintf(percentStr, "%d%%", percent);
    DrawStringOnLcd(215, 620, percentStr, 0xFFFF, &openSans_24);
    DrawProgressBarOnLcd(80, 594, 320, 9, percent, 0x21F4);
    Gd25FlashChipErase();
    if (GetFactoryResult()) {
        ResetBootParam();
    }
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
            f_close(&fp);
        }
        res = f_open(&fp, UPDATE_KEYSTONE3_PATH, FA_OPEN_EXISTING | FA_READ);
        if (res) {
            printf("open error\r\n");
        } else {
            fileSize = f_size(&fp);
            printf("file size=%d\r\n", fileSize);
            if (fileSize > 0) {
                g_availableUpdateFile = true;
            }
            f_close(&fp);
        }
        RecoveryModeMainMenu();
    }
}

void EnterRecoveryMode(void)
{
    while (1) {
        ReducedGlHandler();
        RecoveryHandler();
    }
}

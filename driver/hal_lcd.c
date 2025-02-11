#include "hal_lcd.h"
#include "stdio.h"
#include "string.h"
#include "drv_ili9806.h"
#include "drv_nt35510.h"
#include "drv_power.h"
#include "drv_parallel8080.h"
#include "hardware_version.h"
#include "user_delay.h"

HalLcdOpt_t g_lcdOpt;

void LcdCheck(void)
{
    if (GetHardwareVersion() == VERSION_EVT0) {
        g_lcdOpt.Init = Nt35510Init;
        g_lcdOpt.Draw = Nt35510Draw;
    } else {
        g_lcdOpt.Init = Ili9806Init;
        g_lcdOpt.Draw = Ili9806Draw;
    }
}

void LcdInit(void)
{
    g_lcdOpt.Init();
    LcdFullScreen(0);
}

void LcdOpen(void)
{
    OpenPower(POWER_TYPE_VCC33);
    LcdCheck();
    LcdInit();
    UserDelay(100);
    SetLcdBright(70);
}

bool LcdBusy(void)
{
    return Parallel8080Busy();
}


void LcdDraw(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t *colors)
{
    g_lcdOpt.Draw(xStart, yStart, xEnd, yEnd, colors);
}


void LcdFullScreen(uint16_t color)
{
    uint32_t i;
    uint16_t colors[LCD_DISPLAY_WIDTH], _color;
    _color = (color >> 8) | (color << 8);
    for (i = 0; i < LCD_DISPLAY_WIDTH; i++) {
        colors[i] = _color;
    }
    for (i = 0; i < LCD_DISPLAY_HEIGHT; i++) {
        g_lcdOpt.Draw(0, i, LCD_DISPLAY_WIDTH - 1, i, colors);
        while (LcdBusy());
    }
}


void LcdTest(int argc, char *argv[])
{
    uint32_t color;
    if (strcmp(argv[0], "full_color") == 0) {
        sscanf(argv[1], "%X", &color);
        LcdFullScreen((uint16_t)color);
        printf("lcd full_color=0x%X\r\n", color);
    } else {
        printf("lcd test input err\r\n");
    }
}


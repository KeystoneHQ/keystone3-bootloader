#include "stdio.h"
#include "define.h"
#include "assert.h"
#include "stdio.h"
#include "draw_on_lcd.h"

LV_FONT_DECLARE(openSans_20);

void ShowAssert(const char* file, uint32_t len)
{
    char assertStr[256];
    DeleteAllWidgets();
    PrintOnLcd(&openSans_20, 0xFFFF, "assert,file=%s\nline=%d\n\n", file, len);
    snprintf(assertStr, 256, "assert,file=%s,line=%d", file, len);
    while (1);
}



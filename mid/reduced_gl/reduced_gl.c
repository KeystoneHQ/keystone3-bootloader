#include "reduced_gl.h"
#include "stdio.h"
#include "string.h"
#include "user_delay.h"
#include "hal_lcd.h"
#include "hal_touch.h"
#include "user_memory.h"
#include "draw_on_lcd.h"


LV_FONT_DECLARE(openSans_20);
LV_FONT_DECLARE(openSans_24);

#define TEXT_LINE_GAP               3


static void TouchPadCallback(void);
static DispCallbackFunc_t TouchHandle(uint16_t x, uint16_t y, bool pressed);
static void AddWidget(uint16_t x, uint16_t y, WidgetType type, void *widget);
//static void SetWidgetRefresh(void *widget);
static void SetAllWidgetsRefresh(void);
static void WaitUntilDmaNotBusy(void);
static void DrawWidgets(void);
static void DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
static void DrawButton(WidgetButton_t *pButton, uint16_t x, uint16_t y);
static void DrawLabel(WidgetLabel_t *pLabel, uint16_t x, uint16_t y);
static void DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t bgColor, const lv_font_t *font);
static uint16_t GetTextWidth(const char *text, const lv_font_t *font);
static void DrawLetter(uint16_t x, uint16_t y, uint16_t width, uint16_t height, lv_font_glyph_dsc_t *dsc, const uint8_t *map_p, uint16_t color, uint16_t bgColor);

static TouchStatus_t g_touchStatus;

WidgetNode_t *widgetHead = NULL;


uint16_t _COLOR_MAKE(uint8_t r8, uint8_t g8, uint8_t b8)
{
    uint16_t colorTemp = __COLOR_MAKE(r8, g8, b8);
    return (colorTemp >> 8) | (colorTemp << 8);
}

void ReducedGlInit(void)
{
    TouchInit(TouchPadCallback);
}


static void TouchPadCallback(void)
{
    TouchGetStatus(&g_touchStatus);
}


static void AddWidget(uint16_t x, uint16_t y, WidgetType type, void *widget)
{
    WidgetNode_t *node = widgetHead;
    WidgetNode_t *newNode;
    while (node != NULL) {
        if (node->next == NULL) {
            break;
        }
        node = node->next;
    }
    newNode = pvPortMalloc(sizeof(WidgetNode_t));
    memset(newNode, 0, sizeof(WidgetNode_t));
    if (widgetHead == NULL) {
        widgetHead = newNode;
    } else {
        node->next = newNode;
    }
    newNode->x = x;
    newNode->y = y;
    newNode->type = type;
    newNode->widget = widget;
    newNode->refresh = true;
}


static void SetAllWidgetsRefresh(void)
{
    WidgetNode_t *node = widgetHead;

    while (node != NULL) {
        node->refresh = true;
        node = node->next;
    }
}


void DeleteAllWidgets(void)
{
    WidgetNode_t *node = widgetHead;
    WidgetNode_t *nextNode;
    while (node != NULL) {
        nextNode = node->next;
        if (node->type == WIDGET_TYPE_BUTTON) {
            vPortFree(((WidgetButton_t *)(node->widget))->text);
        } else if (node->type == WIDGET_TYPE_LABEL) {
            vPortFree(((WidgetLabel_t *)(node->widget))->text);
        }
        vPortFree(node->widget);
        vPortFree(node);
        node = nextNode;
    }
    widgetHead = NULL;
    WaitUntilDmaNotBusy();
    LcdFullScreen(0);
}


WidgetButton_t *CreateButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, uint16_t pressedColor, const char *text, DispCallbackFunc_t func)
{
    WidgetButton_t *pWidgetButton = pvPortMalloc(sizeof(WidgetButton_t));
    memset(pWidgetButton, 0, sizeof(WidgetButton_t));
    pWidgetButton->w = w;
    pWidgetButton->h = h;
    pWidgetButton->color = color;
    pWidgetButton->pressedColor = pressedColor;
    pWidgetButton->callbackFunc = func;
    pWidgetButton->text = pvPortMalloc(strlen(text) + 1);
    strcpy(pWidgetButton->text, text);
    AddWidget(x, y, WIDGET_TYPE_BUTTON, pWidgetButton);
    return pWidgetButton;
}


WidgetLabel_t *CreateLabel(uint16_t x, uint16_t y, uint16_t color, const char *text)
{
    WidgetLabel_t *pWidgetLabel = pvPortMalloc(sizeof(WidgetLabel_t));
    memset(pWidgetLabel, 0, sizeof(WidgetLabel_t));
    pWidgetLabel->color = color;
    pWidgetLabel->text = pvPortMalloc(strlen(text) + 1);
    strcpy(pWidgetLabel->text, text);
    AddWidget(x, y, WIDGET_TYPE_LABEL, pWidgetLabel);
    return pWidgetLabel;
}


void SetLabelText(WidgetLabel_t *pWidgetLabel, const char *text)
{
    vPortFree(pWidgetLabel->text);
    pWidgetLabel->text = pvPortMalloc(strlen(text) + 1);
    strcpy(pWidgetLabel->text, text);
    SetAllWidgetsRefresh();
    ReducedGlHandler();
}


void ReducedGlHandler(void)
{
    TouchStatus_t point;
    DispCallbackFunc_t func = NULL;
    static TouchStatus_t lastPoint = {0};
    memcpy(&point, &g_touchStatus, sizeof(TouchStatus_t));
    if (memcmp(&lastPoint, &point, 5) != 0) {
        //printf("x=%d,y=%d,pressed=%d\r\n", point.x, point.y, point.touch);
        memcpy(&lastPoint, &point, sizeof(TouchStatus_t));
        func = TouchHandle(point.x, point.y, point.touch != 0);
    }
    DrawWidgets();
    if (func != NULL) {
        func();
    }
    UserDelay(20);
}


static DispCallbackFunc_t TouchHandle(uint16_t x, uint16_t y, bool pressed)
{
    static uint16_t lastX = 0;
    static uint16_t lastY = 0;
    static bool lastPressed = false;
    static uint16_t pressedX = 0;
    static uint16_t pressedY = 0;
    WidgetNode_t *node = widgetHead;
    WidgetButton_t *pButton;
    DispCallbackFunc_t func = NULL;

    if (lastPressed == false && pressed == true) {
        //press
        pressedX = x;
        pressedY = y;
        while (node != NULL) {
            if (node->type == WIDGET_TYPE_BUTTON) {
                pButton = node->widget;
                if (x >= node->x && x <= (node->x + pButton->w) && y >= node->y && y <= (node->y + pButton->h)) {
                    pButton->pressed = true;
                    node->refresh = true;
                }
            }
            node = node->next;
        }
    } else if (lastPressed == true && pressed == false) {
        //release
        while (node != NULL) {
            if (node->type == WIDGET_TYPE_BUTTON) {
                pButton = node->widget;
                if (lastX >= node->x && lastX <= (node->x + pButton->w) && lastY >= node->y && lastY <= (node->y + pButton->h)) {
                    if (pressedX >= node->x && pressedX <= (node->x + pButton->w) && pressedY >= node->y && pressedY <= (node->y + pButton->h)) {
                        if (pButton->callbackFunc != NULL) {
                            func = pButton->callbackFunc;
                        }
                    }
                }
                pButton->pressed = false;
                node->refresh = true;
            }
            node = node->next;
        }
    }
    lastX = x;
    lastY = y;
    lastPressed = pressed;
    return func;
}


static void WaitUntilDmaNotBusy(void)
{
    while (LcdBusy()) {
        UserDelay(1);
    }
}


static void DrawWidgets(void)
{
    uint32_t nodeCount = 0;
    WidgetNode_t *node = widgetHead;
    while (node != NULL) {
        if (node->refresh) {
            nodeCount++;
            node->refresh = false;
            if (node->type == WIDGET_TYPE_BUTTON) {
                DrawButton((WidgetButton_t *)node->widget, node->x, node->y);
            } else if (node->type == WIDGET_TYPE_LABEL) {
                DrawLabel((WidgetLabel_t *)node->widget, node->x, node->y);
            }
        }
        node = node->next;
    }
}

void SimpleDrawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t buttonColor, const char *text)
{
    uint16_t textX, textY, textPixelLen;

    textPixelLen = GetTextWidth(text, &openSans_20);
    textX = w > textPixelLen ? x + (w - textPixelLen) / 2 : x;
    textY = h > lv_font_get_line_height(&openSans_20) ? y + (h - lv_font_get_line_height(&openSans_20)) / 2 : y;
    DrawRect(x, y, w, h, buttonColor);
    DrawText(textX, textY, text, 0xFFFF, buttonColor, &openSans_20);
}


static void DrawButton(WidgetButton_t *pButton, uint16_t x, uint16_t y)
{
    uint16_t color = 0xFFFF;
    uint16_t textX, textY, textPixelLen;

    textPixelLen = GetTextWidth(pButton->text, &openSans_20);
    textX = pButton->w > textPixelLen ? x + (pButton->w - textPixelLen) / 2 : x;
    textY = pButton->h > lv_font_get_line_height(&openSans_20) ? y + (pButton->h - lv_font_get_line_height(&openSans_20)) / 2 : y;
    if (pButton->pressed) {
        DrawRect(x, y, pButton->w, pButton->h, pButton->pressedColor);
        DrawText(textX, textY, pButton->text, color, pButton->pressedColor, &openSans_20);
    } else {
        DrawRect(x, y, pButton->w, pButton->h, pButton->color);
        DrawText(textX, textY, pButton->text, color, pButton->color, &openSans_20);
    }
}


static void DrawLabel(WidgetLabel_t *pLabel, uint16_t x, uint16_t y)
{
    DrawText(x, y, pLabel->text, pLabel->color, 0, &openSans_24);
}


static void DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    WaitUntilDmaNotBusy();
    uint16_t *colors;
    colors = pvPortMalloc(w * h * sizeof(uint16_t));
    for (uint32_t i = 0; i < w * h; i++) {
        colors[i] = color;
    }
    LcdDraw(x, y, x + w - 1, y + h - 1, (uint16_t *)colors);
    vPortFree(colors);
}


static uint16_t GetTextWidth(const char *text, const lv_font_t *font)
{
    uint16_t width = 0;
    uint32_t len;

    len = strlen(text);
    for (uint32_t i = 0; i < len; i++) {
        width += lv_font_get_glyph_width(font, text[i], text[i + 1]);
    }
    return width;
}


static void DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t bgColor, const lv_font_t *font)
{
    const uint8_t *bitmap;
    uint16_t charWidth, charHeight, xCursor;
    uint32_t len = strlen(text);
    lv_font_glyph_dsc_t dsc;

    charHeight = lv_font_get_line_height(font);
    xCursor = x;
    for (uint32_t i = 0; i < len; i++) {
        charWidth = lv_font_get_glyph_width(font, text[i], text[i + 1]);
        bitmap = lv_font_get_glyph_bitmap(font, text[i]);
        lv_font_get_glyph_dsc(font, &dsc, text[i], text[i + 1]);
        DrawLetter(xCursor, y, charWidth, charHeight, &dsc, bitmap, color, bgColor);
        xCursor += charWidth;
    }
}


typedef struct {
    uint16_t green_h : 3;
    uint16_t red : 5;
    uint16_t blue : 5;
    uint16_t green_l : 3;
} ReducedGlColor_t;


static void DrawLetter(uint16_t x, uint16_t y, uint16_t width, uint16_t height, lv_font_glyph_dsc_t *dsc, const uint8_t *map_p, uint16_t color, uint16_t bgColor)
{
    uint16_t row, col, i, gapX, gapY, gapW, gapH, j;
    uint8_t pixel, r, b, g;
    ReducedGlColor_t color16, *pColor;
    ReducedGlColor_t pixelColor, _bgColor;
    uint32_t maxHeight = height > (height - dsc->box_h - dsc->ofs_y) ? height : (height - dsc->box_h - dsc->ofs_y);
    uint16_t pixelMap[width * maxHeight];

    memcpy(&color16, &color, sizeof(ReducedGlColor_t));
    memcpy(&_bgColor, &bgColor, sizeof(ReducedGlColor_t));
    r = color16.red;
    g = (color16.green_h << 3) + color16.green_l;
    b = color16.blue;

    if (height > dsc->box_h && dsc->box_w > 0 && dsc->box_h > 0) {
        while (LcdBusy());
        for (j = 0; j < (height - dsc->box_h - dsc->ofs_y) * width; j++) {
            pColor = (ReducedGlColor_t *)&pixelMap[j];
            memcpy(pColor, &_bgColor, sizeof(ReducedGlColor_t));
        }
        //memset(pixelMap, 0x00, (height - dsc->box_h - dsc->ofs_y) * width * 2);
        LcdDraw(x, y, x + width - 1, y + (height - dsc->box_h - dsc->ofs_y) - 1, pixelMap);
        while (LcdBusy());
    }
    i = 0;
    for (row = 0; row < dsc->box_h; row++) {
        for (col = 0; col < dsc->box_w; col++) {
            pixel = (map_p[i / 2] >> ((i % 2) == 0 ? 4 : 0)) & 0x0F;
            if (pixel == 0) {
                memcpy(&pixelMap[i], &_bgColor, sizeof(uint16_t));
            } else {
                pixelColor.red = ((r * pixel / 15) & 0x1F);
                pixelColor.green_l = ((g * pixel / 15) & 0x07);
                pixelColor.green_h = (((g * pixel / 15) >> 3) & 0x07);
                pixelColor.blue = ((b * pixel / 15) & 0x1F);
                memcpy(&pixelMap[i], &pixelColor, sizeof(uint16_t));
            }
            i++;
        }
    }
    if (dsc->box_w > 0 && dsc->box_h > 0) {
        while (LcdBusy());
        LcdDraw(x, y + height - dsc->box_h - dsc->ofs_y, x + dsc->box_w - 1, y + height - dsc->ofs_y - 1, pixelMap);
        while (LcdBusy());
        //printf("x=%d,y=%d,box_w=%d,box_h=%d,width=%d\r\n", x, y, dsc->box_w, dsc->box_h, width);
    }
    if (width > dsc->box_w) {
        gapX = x + dsc->box_w;
        gapY = y;
        gapW = width - dsc->box_w;
        gapH = height;
        //wait for DMA send over
        while (LcdBusy());
        for (j = 0; j < gapW * gapH; j++) {
            pColor = (ReducedGlColor_t *)&pixelMap[j];
            memcpy(pColor, &_bgColor, sizeof(ReducedGlColor_t));
        }
        //memset(pixelMap, 0x00, gapW * gapH * 2);
        LcdDraw(gapX, gapY, gapX + gapW - 1, gapY + gapH - 1, pixelMap);
        while (LcdBusy());
        //printf("width=%d,box_w=%d,gapX=%d,gapY=%d,gapW=%d,gapH=%d\r\n", width, dsc->box_w, gapX, gapY, gapW, gapH);
    }
}




#ifndef _REDUCED_GL_H
#define _REDUCED_GL_H

#include "stdint.h"
#include "stdbool.h"


typedef void (*DispCallbackFunc_t)(void);

typedef enum {
    WIDGET_TYPE_UNKNOWN = 0,
    WIDGET_TYPE_BUTTON,
    WIDGET_TYPE_LABEL,
    WIDGET_TYPE_RADIUS_BUTTON,
} WidgetType;


typedef struct __WidgetNode_t {
    WidgetType type;
    uint16_t x;
    uint16_t y;
    bool refresh;
    void *widget;
    struct __WidgetNode_t *next;
} WidgetNode_t;


typedef struct {
    uint16_t w;
    uint16_t h;
    uint16_t color;
    uint16_t pressedColor;
    bool pressed;
    char *text;
    DispCallbackFunc_t callbackFunc;
} WidgetButton_t;


typedef struct {
    uint16_t color;
    char *text;
} WidgetLabel_t;

#define __COLOR_MAKE(r8, g8, b8) (((uint16_t)((r8 >> 3) & 0x1FU) << 11) + ((uint16_t)((g8 >> 2) & 0x3FU) << 5) + (uint16_t)((b8 >> 3) & 0x1FU))

uint16_t _COLOR_MAKE(uint8_t r8, uint8_t g8, uint8_t b8);
void ReducedGlInit(void);
void DeleteAllWidgets(void);
WidgetButton_t *CreateButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, uint16_t pressedColor, const char *text, DispCallbackFunc_t func);
WidgetLabel_t *CreateLabel(uint16_t x, uint16_t y, uint16_t color, const char *text);
void SetLabelText(WidgetLabel_t *pWidgetLabel, const char *text);
void ReducedGlHandler(void);
void SimpleDrawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, const char *text);
WidgetButton_t *CreateRadiusButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, uint16_t pressedColor, const char *text, DispCallbackFunc_t func);


#endif

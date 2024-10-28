#include <stdio.h>
#include "drv_usb.h"
#include "usbd_composite.h"
#include "usbd_usr.h"

__ALIGN_BEGIN USB_OTG_CORE_HANDLE g_usbDev __ALIGN_END;
static bool g_isUsbInit = false;

void UsbInit(void)
{
    USBPHY_CR1_TypeDef usbphy_cr1;

    SYSCTRL_AHBPeriphClockCmd(SYSCTRL_AHBPeriph_USB, ENABLE);
    SYSCTRL_AHBPeriphResetCmd(SYSCTRL_AHBPeriph_USB, ENABLE);


    usbphy_cr1.d32 = SYSCTRL->USBPHY_CR1;

    usbphy_cr1.b.commononn           = 0;
    usbphy_cr1.b.stop_ck_for_suspend = 0;

    SYSCTRL->USBPHY_CR1 = usbphy_cr1.d32;

    memset(&g_usbDev, 0x00, sizeof(g_usbDev));

    g_isUsbInit = true;
    USBD_Init(&g_usbDev, USB_OTG_FS_CORE_ID, &USR_desc, DeviceCallback, &USRD_cb);
}

void USB_IRQHandler(void)
{
    //NVIC_DisableIRQ(USB_IRQn);
    //PubValueMsg(USB_MSG_ISR_HANDLER, (uint32_t)&g_usbDev);
    USBD_OTG_ISR_Handler(&g_usbDev);
}

void UsbLoop(void)
{

}


void UsbDeInit(void)
{
    if (g_isUsbInit) {
        USBD_DeInit(&g_usbDev);
    }
}


static uint8_t g_sendBuf[512];
static uint32_t g_sendIndex, g_sendTotal;

void WebUsbSend(const void *data, uint32_t length)
{
    memset(g_sendBuf, 0, sizeof(g_sendBuf));
    memcpy(g_sendBuf, data, length);
    g_sendIndex = 0;
    g_sendTotal = length;
    DCD_EP_Tx(&g_usbDev, CDC_IN_EP, g_sendBuf, 64);
}

void WebUsbSendCallback(void)
{
    g_sendIndex += 64;
    if (g_sendIndex >= g_sendTotal) {
        return;
    }
    DCD_EP_Tx(&g_usbDev, CDC_IN_EP, g_sendBuf + g_sendIndex, 64);
}


void WebUsbRcvCallback(const uint8_t *rcvData, uint32_t length)
{
}


#include <string.h>
#include <stdio.h>
#include "mhscpu.h"
#include "cm_backtrace.h"
#include "drv_sys.h"
#include "drv_uart.h"
#include "drv_spi.h"
#include "drv_trng.h"
#include "drv_ds28s60.h"
#include "drv_gd25qxx.h"
#include "drv_battery.h"
#include "drv_qspi_flash.h"
#include "drv_lcd_bright.h"
#include "drv_power.h"
#include "user_fatfs.h"
#include "firmware_update.h"
#include "drv_usb.h"
#include "check_app.h"
#include "recovery_mode.h"
#include "cm_backtrace.h"

#define TAMPER_ENABLE

#ifdef TAMPER_ENABLE
static bool ReadTamperInput(void);
#endif
void CmdIsrRcvByte(uint8_t byte);
static void JumpToApp(void);


int main(void)
{
    SystemClockInit();
    SetLcdBright(0);
    PowerInit();

#ifdef TAMPER_ENABLE
    if (ReadTamperInput() == false) {
        JumpToApp();
        while (1);
    }
#endif
    Uart0Init(CmdIsrRcvByte);
    cm_backtrace_init("bootloader", "V1.0.0", "V1.0.0");
    TrngInit();
    Gd25FlashInit();
    QspiFlashInit();
    PrintSystemInfo();
    MountUsbFatfs();
    CopyBin2Flash();
    if (OptionToRecoveryMode()) {
        RecoveryMode();
    }
    FirmwareUpdate("1:pillar.bin");
    FirmwareUpdate("1:keystone3.bin");
    if (CheckApp() == false || CheckAppExist() == false) {
        RecoveryMode();
    }
    
#if (SIGNATURE_ENABLE == 1)
    CalculateCheckSum();
#endif

    JumpToApp();
    while (1);
}


int _write(int fd, char *pBuffer, int size)
{
    for (int i = 0; i < size; i++) {
        while (!UART_IsTXEmpty(UART0));
#ifdef __GNUC__
        // UART_SendData(UART0, '-');
        UART_SendData(UART0, (uint8_t) pBuffer[i]);
#else
        UART_SendData(UART0, (uint8_t) pBuffer[i]);
#endif
    }
    return size;
}

int fputc(int ch, FILE *f)
{
    (void)(f);                              //unused arg
    while (!UART_IsTXEmpty(UART0));
    UART_SendData(UART0, (uint8_t) ch);

    return ch;
}



void CmdIsrRcvByte(uint8_t byte)
{
    static uint32_t rxF8Count = 0;
    if (byte == 0xF8) {
        if (rxF8Count++ > 10) {
            NVIC_SystemReset();
        }
    } else {
        rxF8Count = 0;
    }
}


static void JumpToApp(void)
{
    typedef int (*jumpApp)(void);
    volatile int *ptr = (int *)APP_ADDR;
    jumpApp app;

    if (*ptr != 0xffffffff) {
        app = (jumpApp)(*(__IO uint32_t*)(APP_ADDR + 4));
        __disable_irq();
        __set_MSP(*(__IO uint32_t*) APP_ADDR);

        app();
    }
}


#ifdef TAMPER_ENABLE
static bool ReadTamperInput(void)
{
    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpioInit.GPIO_Pin = GPIO_Pin_2;
    gpioInit.GPIO_Remap = GPIO_Remap_1;
    GPIO_Init(GPIOA, &gpioInit);

    return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == SET;
}
#endif


#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

    /* Infinite loop */
    printf("err,file=%s,line=%d\r\n", (char *)file, line);
    while (1);
}
#endif

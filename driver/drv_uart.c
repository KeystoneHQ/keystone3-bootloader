#include "drv_uart.h"
#include "mhscpu.h"
#include "define.h"

static UartRcvByteCallbackFunc_t g_uartRcvByteCallback;

void Uart0Init(UartRcvByteCallbackFunc_t func)
{
    UART_InitTypeDef UART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    SYSCTRL_APBPeriphClockCmd(SYSCTRL_APBPeriph_UART0 | SYSCTRL_APBPeriph_GPIO, ENABLE);
    SYSCTRL_APBPeriphResetCmd(SYSCTRL_APBPeriph_UART0, ENABLE);

    if (func != NULL) {
        g_uartRcvByteCallback = func;
    }
    GPIO_PinRemapConfig(GPIOA, GPIO_Pin_0 | GPIO_Pin_1, GPIO_Remap_0);

    UART_InitStructure.UART_BaudRate = 512000;
    UART_InitStructure.UART_WordLength = UART_WordLength_8b;
    UART_InitStructure.UART_StopBits = UART_StopBits_1;
    UART_InitStructure.UART_Parity = UART_Parity_No;

    UART_Init(UART0, &UART_InitStructure);

    UART_ITConfig(UART0, UART_IT_RX_RECVD, ENABLE);

    NVIC_SetPriorityGrouping(NVIC_PriorityGroup_0);

    NVIC_InitStructure.NVIC_IRQChannel = UART0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}


void UART0_IRQHandler(void)
{
    volatile uint32_t iir;
    uint8_t byte;

    UART_TypeDef * UARTx = UART0;

    iir = UART_GetITIdentity(UARTx);

    switch (iir & 0x0f) {
    case UART_IT_ID_RX_RECVD: {
        byte = UART_ReceiveData(UARTx);
        if (g_uartRcvByteCallback) {
            g_uartRcvByteCallback(byte);
        }
    }
    break;
    case UART_IT_ID_TX_EMPTY: {
    }
    break;
    case UART_IT_ID_MODEM_STATUS: {
        volatile uint32_t msr = UARTx->MSR;
        UNUSED(msr);
    }
    break;
    case UART_IT_ID_LINE_STATUS: {
        volatile uint32_t lsr = UARTx->LSR;
        UNUSED(lsr);
    }
    break;
    case UART_IT_ID_BUSY_DETECT: {
        volatile uint32_t usr = UARTx->USR;
        UNUSED(usr);
    }
    break;
    case UART_IT_ID_CHAR_TIMEOUT: {
        volatile uint32_t rbr = UART_ReceiveData(UARTx);
        UNUSED(rbr);
    }
    break;
    default:
        break;

    }
}



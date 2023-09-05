/************************ (C) COPYRIGHT Megahuntmicro *************************
 * @file                : mhscpu_adc.c
 * @author              : Megahuntmicro
 * @version             : V1.0.0
 * @date                : 21-October-2014
 * @brief               : This file provides all the ADC firmware functions
 *****************************************************************************/

/* Include ------------------------------------------------------------------*/
#include "mhscpu_adc.h"
/* Private typedef ----------------------------------------------------------*/
/* Private define -----------------------------------------------------------*/
/* Private macro ------------------------------------------------------------*/
/* Private variables --------------------------------------------------------*/
/* Ptivate function prototypes ----------------------------------------------*/


/******************************************************************************
* Function Name  : ADC_Init
* Description    : Initialize ADC, initialize reference value
* Input          : ADC_InitStruct：pointer to the data structure to initialize
* Return         : NONE
******************************************************************************/
void ADC_Init(ADC_InitTypeDef *ADC_InitStruct)
{
    assert_param(IS_ADC_CHANNEL(ADC_InitStruct->ADC_Channel));
    assert_param(IS_ADC_SAMP(ADC_InitStruct->ADC_SampSpeed));
    assert_param(IS_FUNCTIONAL_STATE(ADC_InitStruct->ADC_IRQ_EN));
    assert_param(IS_FUNCTIONAL_STATE(ADC_InitStruct->ADC_FIFO_EN));

    /* Select ADC Channel */
    ADC0->ADC_CR1 = (ADC0->ADC_CR1 & ~(ADC_CR1_CHANNEL_MASK)) | ADC_InitStruct->ADC_Channel;

    /* Select ADC Channel Samping */
    ADC0->ADC_CR1 = (ADC0->ADC_CR1 & ~(ADC_CR1_SAMPLE_RATE_MASK)) | (ADC_InitStruct->ADC_SampSpeed << ADC_CR1_SAMPLE_RATE_Pos);

    /* Set ADC Interrupt */
    if (ENABLE == ADC_InitStruct->ADC_IRQ_EN) {
        ADC0->ADC_CR1 |= ADC_CR1_IRQ_ENABLE;
    } else {
        ADC0->ADC_CR1 &= ~ADC_CR1_IRQ_ENABLE;
    }

    /* Set ADC FIFO */
    if (ENABLE == ADC_InitStruct->ADC_FIFO_EN) {
        ADC0->ADC_FIFO |= ADC_FIFO_ENABLE;
    } else {
        ADC0->ADC_FIFO &= ~ADC_FIFO_ENABLE;
    }
}

/******************************************************************************
* Function Name  : ADC_GetResult
* Description    : Immediately obtain the value of the corresponding channel of the ADC, with timeout detection.
* Input          : None
* Return         : 0:time out  Other:ADC value
******************************************************************************/
int32_t ADC_GetResult(void)
{
    while ((!(ADC0->ADC_SR & ADC_SR_DONE_FLAG)));

    return ADC0->ADC_DATA & 0xFFF;
}

/******************************************************************************
* Function Name  : ADC_GetFIFOResult
* Description    : Immediately obtain the value of the corresponding channel of the ADC, with timeout detection.
* Input          : ADCdata:FIFO
                   len
* Return         : ADC value in FIFO
******************************************************************************/
int32_t ADC_GetFIFOResult(uint16_t *ADCdata, uint32_t len)
{
    uint32_t i, adccount = 0;

    //Get the number of lengths greater than the FIFO threshold
    if (NULL == ADCdata) {
        return 0;
    }

    adccount = ADC_GetFIFOCount();
    if (adccount > len) {
        adccount = len;
    }

    for (i = 0; i < adccount; i++) {
        ADCdata[i] = ADC0->ADC_DATA & 0xFFF;
    }

    return adccount;
}

/******************************************************************************
* Function Name  : ADC_GetFIFOCount
* Description    : Get the number of data in FIFO.
* Input          : None
* Return         : 0: FIFO mode is not enabled other: the number of data in FIFO.
******************************************************************************/
int32_t ADC_GetFIFOCount(void)
{
    if ((ADC0->ADC_FIFO & ADC_FIFO_ENABLE) != ADC_FIFO_ENABLE) {
        return 0;
    }

    return ADC0->ADC_FIFO_FL & 0x1F;
}

/******************************************************************************
* Function Name  : ADC_FifoReset
* Description    : Reset ADC fifo
* Input          : None
* Return         : None
******************************************************************************/
void ADC_FIFOReset(void)
{
    ADC0->ADC_FIFO |= ADC_FIFO_RESET;
}

/******************************************************************************
* Function Name  : ADC_ITCmd
* Description    : Switch to control ADC interrupt.
* Input          : NewState：ENABLE/DISABLE
* Output         : NONE
* Return         : NONE
******************************************************************************/
void ADC_ITCmd(FunctionalState NewState)
{
    assert_param(IS_FUNCTIONAL_STATE(NewState));

    if (DISABLE != NewState) {
        ADC0->ADC_CR1 |= ADC_CR1_IRQ_ENABLE;
    } else {
        ADC0->ADC_CR1 &= ~ADC_CR1_IRQ_ENABLE;
    }
}

/******************************************************************************
* Function Name  : ADC_FIFOOverflowITcmd
* Description    : Switch to control ADC FIFO overflow interrupt.
* Input          : NewState：ENABLE/DISABLE
* Output         : NONE
* Return         : NONE
******************************************************************************/
void ADC_FIFOOverflowITcmd(FunctionalState NewState)
{
    assert_param(IS_FUNCTIONAL_STATE(NewState));

    if (DISABLE != NewState) {
        ADC0->ADC_FIFO |= ADC_FIFO_OV_INT_ENABLE;
    } else {
        ADC0->ADC_FIFO &= ~ADC_FIFO_OV_INT_ENABLE;
    }
}

/******************************************************************************
* Function Name  : ADC_StartCmd
* Description    : ADC conversion start control
* Input          : NewState：ENABLE/DISABLE
* Output         : NONE
* Return         : NONE
******************************************************************************/
void ADC_StartCmd(FunctionalState NewState)
{
    if (DISABLE != NewState) {
        ADC0->ADC_CR1 |= ADC_CR1_SAMP_ENABLE;
        ADC0->ADC_CR1 &= ~ADC_CR1_POWER_DOWN;
    } else {
        ADC0->ADC_CR1 &= ~ADC_CR1_SAMP_ENABLE;
        ADC0->ADC_CR1 |= ADC_CR1_POWER_DOWN;
    }
}

/******************************************************************************
* Function Name  : ADC_FIFODeepth
* Description    : ADC fifo deep setting
* Input          : FIFO_Deepth max 0x20
* Output         : NONE
* Return         : NONE
******************************************************************************/
void ADC_FIFODeepth(uint32_t FIFO_Deepth)
{
    ADC0->ADC_FIFO_THR = FIFO_Deepth - 1;
}

/******************************************************************************
* Function Name  : ADC_CalVoltage
* Description    : Calculate the converted voltage value
* Input          : u32ADC_Value
*                : u32ADC_Ref_Value
* Output         : NONE
* Return         : The converted voltage value
******************************************************************************/
uint32_t ADC_CalVoltage(uint32_t u32ADC_Value, uint32_t u32ADC_Ref_Value)
{
    return (u32ADC_Value * u32ADC_Ref_Value / 4095);
}

/******************************************************************************
* Function Name  : ADC_BuffCmd
* Description    : ADC Buff enable
* Input          : NewState：ENABLE/DISABLE
* Output         : NONE
* Return         : NONE
******************************************************************************/
void ADC_BuffCmd(FunctionalState NewState)
{
    assert_param(IS_FUNCTIONAL_STATE(NewState));

    if (DISABLE != NewState) {
        ADC0->ADC_CR2 &= ~ADC_CR2_BUFF_ENABLE;
    } else {
        ADC0->ADC_CR2 |= ADC_CR2_BUFF_ENABLE;
    }
}

/******************************************************************************
* Function Name  : ADC_ChannelSwitch
* Description    : Change ADC sampling channel.
* Input          : ADC_ChxTypeDef:ADC corresponding channel.
* Return         : NONE
******************************************************************************/
void ADC_ChannelSwitch(ADC_ChxTypeDef Channelx)
{
    uint8_t i;

    assert_param(IS_ADC_CHANNEL(Channelx));
    ADC0->ADC_CR1 = (ADC0->ADC_CR1 & ~(ADC_CR1_CHANNEL_MASK)) | Channelx;
    for (i = 0; i < 7; i++) {
        ADC_GetResult();
    }
}
/************************ (C) COPYRIGHT 2017 Megahuntmicro ****END OF FILE****/

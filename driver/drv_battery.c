#include "drv_battery.h"
#include "mhscpu.h"
#include "stdio.h"
#include "stdlib.h"


#define BATTERY_ADC_TIMES       100


/// @brief Battery init, including ADC init.
/// @param
void BatteryInit(void)
{
    ADC_InitTypeDef ADC_InitStruct;

    SYSCTRL_APBPeriphClockCmd(SYSCTRL_APBPeriph_GPIO | SYSCTRL_APBPeriph_ADC, ENABLE);
    SYSCTRL_APBPeriphResetCmd(SYSCTRL_APBPeriph_ADC, ENABLE);

    GPIO_PinRemapConfig(GPIOC, GPIO_Pin_4, GPIO_Remap_2);
    GPIO_PullUpCmd(GPIOC, GPIO_Pin_4, DISABLE);

    ADC_InitStruct.ADC_Channel = ADC_CHANNEL_4;
    ADC_InitStruct.ADC_SampSpeed = ADC_SpeedPrescaler_2;
    ADC_InitStruct.ADC_IRQ_EN = DISABLE;
    ADC_InitStruct.ADC_FIFO_EN = DISABLE;

    ADC_Init(&ADC_InitStruct);
}



/// @brief Get battery voltage.
/// @param
/// @return Battery voltage, in millivolts.
uint32_t GetBatteryMilliVolt(void)
{
    int32_t i, adcAver;
    uint64_t adcSum = 0;

    ADC_StartCmd(ENABLE);
    for (i = 0; i < BATTERY_ADC_TIMES; i++) {
        adcSum += ADC_GetResult();
    }
    ADC_StartCmd(DISABLE);
    adcAver = adcSum / BATTERY_ADC_TIMES;

    return ADC_CalVoltage(adcAver, 1880);
}



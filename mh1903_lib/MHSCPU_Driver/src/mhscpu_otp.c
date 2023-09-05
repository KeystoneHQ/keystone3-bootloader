/************************ (C) COPYRIGHT Megahuntmicro *************************
 * @file                : mhscpu_otp.c
 * @author              : Megahuntmicro
 * @version             : V1.0.0
 * @date                : 21-October-2014
 * @brief               : This file provides all the OTP firmware functions
 *****************************************************************************/

/* Include ------------------------------------------------------------------*/
#include "mhscpu_otp.h"


#define OTP_DONE_FLAG               BIT(0)
#define OTP_START                   BIT(0)
#define OTP_WAKEUPEN                BIT(1)

#define OTP_TIM_EN                  (0xA5)

/*OTP unlock key*/
#define OTP_KEY1                    (0xABCD00A5)
#define OTP_KEY2                    (0x1234005A)


static uint32_t gu32OTP_Key1 = 0;
static uint32_t gu32OTP_Key2 = 0;


/******************************************************************************
* Function Name  : OTP_Operate
* Description    :
* Input          : NONE
* Return         : NONE
******************************************************************************/
static void OTP_Operate(void)
{
    OTP->CS |= OTP_START;
}

/******************************************************************************
 * Function Name  : OTP_WakeUp
 * Description    : Wake up form low power mode for OTP.
 * Input          : NONE
 * Return         : NONE
******************************************************************************/
void OTP_WakeUp(void)
{
    OTP->CFG |= OTP_WAKEUPEN;
}

/******************************************************************************
 * Function Name  : OTP_SetLatency
 * Description    : Set the OTP timing register, and set the number of clocks of 1Us and 10ns respectively.
                    If 0 is passed in, a value is calculated according to the current clock frequency and written into it.
                    When the otp_tim_en register is A5 (when the OTP_TimCmd() function passes in ENABLE),
                    only use the configuration of this function, otherwise (OTP_TimCmd() function passes DISENABLE)
                    Use your own configuration in the kernel, it is recommended to use the kernel's own configuration (the OTP_TimCmd() function is passed to DISENABLE).
 * Input          : u8_1UsClk,u8_10NsCLK
 * Return         : NONE
******************************************************************************/
void OTP_SetLatency(uint8_t u8_1UsClk, uint8_t u8_10NsCLK)
{
    if (0 == u8_1UsClk) {
        OTP->TIM = ((OTP->TIM & ~(0xFF)) | (SYSCTRL->HCLK_1MS_VAL + 1000 - 1) / 1000);
    } else {
        OTP->TIM = ((OTP->TIM & ~(0xFF)) | u8_1UsClk);
    }
    if (0 == u8_10NsCLK) {
        OTP->TIM = ((OTP->TIM & ~(0x7 << 8)) | (SYSCTRL->HCLK_1MS_VAL / 100000 + 1));
    } else {
        OTP->TIM = ((OTP->TIM & ~(0x7 << 8)) | u8_10NsCLK);
    }
}

/******************************************************************************
* Function Name  : OTP_Unlock
* Description    : Get the key used for the write operation.
* Input          : NONE
* Return         : NONE
******************************************************************************/
void OTP_Unlock(void)
{
    gu32OTP_Key1 = OTP_KEY1;
    gu32OTP_Key2 = OTP_KEY2;
}

/******************************************************************************
* Function Name  : OTP_Lock
* Description    : lock for write operations.
* Input          : NONE
* Return         : NONE
******************************************************************************/
void OTP_Lock(void)
{
    gu32OTP_Key1 = ~OTP_KEY1;
    gu32OTP_Key2 = ~OTP_KEY2;
    OTP->PROT = OTP_KEY2;
}

/******************************************************************************
* Function Name  : OTP_IsWriteDone
* Description    : Determine whether the OTP write operation is complete.
* Input          : NONE
* Return         : Boolean:FALSE/TRUE
******************************************************************************/
Boolean OTP_IsWriteDone(void)
{
    if ((OTP->CS & OTP_DONE_FLAG) == OTP_DONE_FLAG) {
        return FALSE;
    } else {
        return TRUE;
    }
}

/******************************************************************************
 * Function Name  : OTP_GetFlag
 * Description    : Get the status after the operation is completed.
 * Input          : NONE
 * Return         : OTP_StatusTypeDef:
                    OTP_Complete
                    OTP_ReadOnProgramOrSleep
                    OTP_ProgramIn_HiddenOrRO_Block
                    OTP_ProgramOutOfAddr
                    OTP_ProgramOnSleep
                    OTP_WakeUpOnNoSleep
******************************************************************************/
OTP_StatusTypeDef OTP_GetFlag(void)
{
    return (OTP_StatusTypeDef)((OTP->CS >> 1) & 0x7);
}

/******************************************************************************
 * Function Name  : OTP_ClearStatus
 * Description    : Clear the value of the status register.
 * Input          : NONE
 * Return         : NONE
******************************************************************************/
void OTP_ClearStatus(void)
{
    OTP->CS &= ~(0x07 << 1);
}

/******************************************************************************
* Function Name  : WriteOtpWord
* Description    : OTP area programming.
* Input          : u32Addr
                   u32Data
* Return         : OTP_StatusTypeDef
******************************************************************************/
OTP_StatusTypeDef OTP_WriteWord(uint32_t addr, uint32_t w)
{
    uint32_t Delay = 0;
    OTP_StatusTypeDef otp_status;

    assert_param(IS_OTP_ADDRESS(addr));
    assert_param(0 == (addr & 0x03));

    OTP->PDATA = w;
    OTP->ADDR = addr;

    OTP->PROT = gu32OTP_Key1;
    OTP->PROT = gu32OTP_Key2;

    OTP_Operate();

    Delay = 0xFFFF;
    while ((OTP_IsWriteDone() == FALSE) && (0 != --Delay));
    if (0 == Delay) {
        return OTP_TimeOut;
    }

    otp_status = OTP_GetFlag();
    if (OTP_Complete != otp_status) {
        OTP_ClearStatus();
        return otp_status;
    }

    while (FALSE == OTP_IsReadReady());
    if ((*(uint32_t *)addr) != w) {
        return OTP_DataWrong;
    }

    return OTP_Complete;
}

/******************************************************************************
* Function Name  : OTP_TimCmd
* Description    : Whether to enable the otp_tim register.
* Input          : FunctionalState
* Return         : NONE
******************************************************************************/
void OTP_TimCmd(FunctionalState NewState)
{
    assert_param(IS_FUNCTIONAL_STATE(NewState));

    if (ENABLE == NewState) {
        OTP->TIM_EN = OTP_TIM_EN;
    } else {
        OTP->TIM_EN = ~OTP_TIM_EN;
    }
}

/******************************************************************************
* Function Name  : OTP_GetProtect
* Description    : Whether to read the entire OTP area is read-only.
* Input          : NONE
* Return         : Returns the status of the read-only lock.
******************************************************************************/
uint32_t OTP_GetProtect(void)
{
    return OTP->RO;
}

/******************************************************************************
* Function Name  : OTP_GetProtectLock
* Description    : Read the state of the hardware lock corresponding to the read-only lock.
* Input          : NONE
* Return         : The state of the hardware lock corresponding to the read-only lock.
******************************************************************************/
uint32_t OTP_GetProtectLock(void)
{
    return OTP->ROL;
}

/******************************************************************************
* Function Name  : OTP_SetProtect
* Description    : Set the corresponding OTP address to read-only.
* Input          : u32Addr
* Return         : NONE
******************************************************************************/
void OTP_SetProtect(uint32_t u32Addr)
{
    uint32_t pu32RO;
    assert_param(IS_OTP_ADDRESS(u32Addr));

    pu32RO = (u32Addr - OTP_BASE) / 0x100;

    OTP->RO |= BIT(pu32RO);
}

/******************************************************************************
* Function Name  : OTP_SetProtectLock
* Description    : Set the hardware lock of the read-only lock of the register corresponding to the OTP address.
                   After setting to 1, the software cannot clear 0, and the hardware will automatically clear 0 after reset.
* Input          : u32Addr
* Return         : NONE
******************************************************************************/
void OTP_SetProtectLock(uint32_t u32Addr)
{
    uint32_t pu32ROL;
    assert_param(IS_OTP_ADDRESS(u32Addr));

    OTP_SetProtect(u32Addr);
    pu32ROL = (u32Addr - OTP_BASE) / 0x100;

    OTP->ROL |= BIT(pu32ROL);
}

/******************************************************************************
* Function Name  : OTP_UnProtect
* Description    : Set the corresponding OTP address as rewritable.
* Input          : u32Addr
* Return         : NONE
******************************************************************************/
void OTP_UnProtect(uint32_t u32Addr)
{
    uint32_t pu32RO;
    assert_param(IS_OTP_ADDRESS(u32Addr));

    pu32RO = (u32Addr - OTP_BASE) / 0x100;

    OTP->RO &= ~BIT(pu32RO);
}

/******************************************************************************
* Function Name  : OTP_IsReadReady
* Description    : Is it possible to read operation, OTP is not readable when it is in programming/sleep state.
* Input          : void
* Return         : Boolean:TRUE/FALSE
******************************************************************************/
Boolean OTP_IsReadReady(void)
{
    if (BIT(31) == (OTP->CS & BIT(31))) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/******************************************************************************
* Function Name  : OTP_IsProtect
* Description    : Whether the read-only lock for reading the corresponding address is locked.
* Input          : u32Addr
* Return         : Boolean:TRUE/FALSE
******************************************************************************/
Boolean OTP_IsProtect(uint32_t u32Addr)
{
    uint32_t pu32RO;

    assert_param(IS_OTP_ADDRESS(u32Addr));

    pu32RO = (u32Addr - OTP_BASE) / 0x100;

    if (BIT(pu32RO) == (OTP->RO & BIT(pu32RO))) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/******************************************************************************
* Function Name  : OTP_IsProtectLock
* Description    : Whether the hardware lock of the read-only lock that reads the corresponding address is locked
* Input          : u32Addr
* Return         : Boolean:TRUE/FALSE
******************************************************************************/
Boolean OTP_IsProtectLock(uint32_t u32Addr)
{
    uint32_t pu32ROL;

    assert_param(IS_OTP_ADDRESS(u32Addr));

    pu32ROL = (u32Addr - OTP_BASE) / 0x100;

    if (BIT(pu32ROL) == (OTP->ROL & BIT(pu32ROL))) {
        return TRUE;
    } else {
        return FALSE;
    }
}


/******************************************************************************
* Function Name  : OTP_PowerOn
* Description    : Turn on the OTP power supply, you need to wait 2us after power-on to read the OTP content.
* Input          :
* Return         :
******************************************************************************/
void OTP_PowerOn(void)
{
    uint32_t n = SYSCTRL->HCLK_1MS_VAL / 250;

    SYSCTRL->LDO25_CR &= ~BIT5;     //Turn on the OTP power supply
    while (n--);
    SYSCTRL_AHBPeriphClockCmd(SYSCTRL_AHBPeriph_OTP, ENABLE);
}

/************************ (C) COPYRIGHT 2014 Megahuntmicro ****END OF FILE****/

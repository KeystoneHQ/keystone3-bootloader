/**************************************************************************************************
 * Copyright (c) keyst.one. 2020-2025. All rights reserved.
 * Description: MAXIM DS28S60
 * Author: leon sun
 * Create: 2022-12-1
 ************************************************************************************************/

#include "drv_ds28s60.h"
#include "stdint.h"
#include "stdbool.h"
#include "stdio.h"
#include "string.h"
#include "mhscpu.h"
//#include "drv_spi.h"
#include "drv_spi_io.h"
#include "user_memory.h"
#include "log_print.h"
#include "sha256_hmac.h"
#include "drv_trng.h"
#include "user_utils.h"
#include "user_delay.h"
#include "drv_otp.h"
#include "err_code.h"
#include "mhscpu_otp.h"

//#define DS28S60_TEST_MODE
//#define DS28S60_FORCE_BINDING

#define DS28S60_HARDWARE_EVB            0
#define DS28S60_HARDWARE_EVT0           1

#define DS28S60_HARDWARE_CFG            DS28S60_HARDWARE_EVT0


//EVB
#if (DS28S60_HARDWARE_CFG == DS28S60_HARDWARE_EVB)
#define DS28S60_CS_PORT                 GPIOD
#define DS28S60_CS_PIN                  GPIO_Pin_7
#define DS28S60_PDWN_PORT               GPIOG
#define DS28S60_PDWN_PIN                GPIO_Pin_0
#define DS28S60_MISO_PORT               GPIOB
#define DS28S60_MISO_PIN                GPIO_Pin_5
#define DS28S60_MOSI_PORT               GPIOB
#define DS28S60_MOSI_PIN                GPIO_Pin_4
#define DS28S60_CLK_PORT                GPIOB
#define DS28S60_CLK_PIN                 GPIO_Pin_2

#elif (DS28S60_HARDWARE_CFG == DS28S60_HARDWARE_EVT0)
//EVT0
#define DS28S60_CS_PORT                 GPIOD
#define DS28S60_CS_PIN                  GPIO_Pin_9
#define DS28S60_PDWN_PORT               GPIOG
#define DS28S60_PDWN_PIN                GPIO_Pin_0
#define DS28S60_MISO_PORT               GPIOD
#define DS28S60_MISO_PIN                GPIO_Pin_11
#define DS28S60_MOSI_PORT               GPIOD
#define DS28S60_MOSI_PIN                GPIO_Pin_10
#define DS28S60_CLK_PORT                GPIOD
#define DS28S60_CLK_PIN                 GPIO_Pin_8

#endif

#define WAIT_DELAY_TICK                 50
#define RETRY_MAX_COUNT                 5

#define DS28S60_CS_SET                  GPIO_SetBits(DS28S60_CS_PORT, DS28S60_CS_PIN)
#define DS28S60_CS_CLR                  GPIO_ResetBits(DS28S60_CS_PORT, DS28S60_CS_PIN)

#define DS28S60_PDWN_SET                GPIO_SetBits(DS28S60_PDWN_PORT, DS28S60_PDWN_PIN)
#define DS28S60_PDWN_CLR                GPIO_ResetBits(DS28S60_PDWN_PORT, DS28S60_PDWN_PIN)

#define DS28S60_OVERTIME                1000
#define BINDING_DATA_PAGE               91

#define VALUE_CHECK(value, expect)          {if (value != expect) {printf("input err!\r\n"); return; }}


DS28S60_Info_t g_ds28s60Info;

static const SPIIO_Cfg_t DS28S60_SPI_CONFIG = {
    .MISO_PORT = DS28S60_MISO_PORT,
    .MISO_PIN = DS28S60_MISO_PIN,
    .MOSI_PORT = DS28S60_MOSI_PORT,
    .MOSI_PIN = DS28S60_MOSI_PIN,
    .CLK_PORT = DS28S60_CLK_PORT,
    .CLK_PIN = DS28S60_CLK_PIN,
};

#ifdef DS28S60_TEST_MODE

static const uint8_t MASTER_SECRET[] = {
    0x3B, 0x01, 0x5F, 0x84, 0x13, 0x1B, 0x10, 0xDB, \
    0x43, 0x52, 0xC3, 0x1F, 0xE6, 0x0A, 0x37, 0x1E, \
    0x58, 0xFB, 0x63, 0x9A, 0x64, 0xC3, 0x13, 0xF2, \
    0x2D, 0x65, 0xE9, 0xB4, 0xC5, 0x91, 0x3E, 0x15
};

static const uint8_t BINDING_PAGE_DATA[] = {
    0x6E, 0x07, 0x52, 0xCB, 0xEB, 0x52, 0xDF, 0x38, \
    0xB9, 0xFE, 0x05, 0x76, 0x67, 0xAE, 0xA5, 0x07, \
    0xF2, 0xEF, 0x4A, 0xCB, 0x6A, 0x1E, 0x92, 0x9A, \
    0xF7, 0xAE, 0xB0, 0x92, 0x0F, 0x25, 0xC3, 0xAF
};

static const uint8_t PARTIAL_SECRET[] = {
    0x1F, 0xE9, 0xE0, 0x11, 0x3F, 0x6C, 0xF0, 0x82, \
    0x08, 0xC5, 0x3F, 0x9B, 0x9E, 0x85, 0x7A, 0xE6, \
    0x88, 0x7D, 0xE1, 0x43, 0xD9, 0x70, 0x05, 0x1C, \
    0x87, 0x83, 0x7B, 0x34, 0xED, 0x03, 0x2E, 0x62
};
#else
#define MASTER_SECRET_ADDR          OTP_ADDR_DS28S60
#define BINDING_PAGE_DATA_ADDR      MASTER_SECRET_ADDR + 32
#define PARTIAL_SECRET_ADDR         BINDING_PAGE_DATA_ADDR + 32
#endif

static int32_t DS28S60_GetInfo(void);
static void DS28S60_GetHmacKey(uint8_t *key, const DS28S60_Info_t *info, uint8_t pg, uint8_t cmd);
static int32_t DS28S60_SendCmdAndGetResult(uint8_t cmd, uint8_t *para, uint8_t paraLen, uint8_t expectedLen, uint8_t *resultArray);
static int32_t DS28S60_TrySendCmdAndGetResult(uint8_t cmd, uint8_t *para, uint8_t paraLen, uint8_t expectedLen, uint8_t *resultArray);
static void GetMasterSecret(uint8_t *masterSecret);
static void GetBindingPageData(uint8_t *bindingPageData);
static void GetPartialSecret(uint8_t *partialSecret);


void DS28S60_Init(void)
{
    GPIO_InitTypeDef gpioInit = {0};

    SpiIoInit(&DS28S60_SPI_CONFIG);
    gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
    gpioInit.GPIO_Pin = DS28S60_CS_PIN;
    gpioInit.GPIO_Remap = GPIO_Remap_1;
    GPIO_Init(DS28S60_CS_PORT, &gpioInit);

    gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
    gpioInit.GPIO_Pin = DS28S60_PDWN_PIN;
    gpioInit.GPIO_Remap = GPIO_Remap_1;
    GPIO_Init(DS28S60_PDWN_PORT, &gpioInit);
    DS28S60_CS_SET;
    DS28S60_PDWN_SET;
    UserDelay(100);
}


void DS28S60_Open(void)
{
    GPIO_InitTypeDef gpioInit = {0};

    SpiIoInit(&DS28S60_SPI_CONFIG);
    gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
    gpioInit.GPIO_Pin = DS28S60_CS_PIN;
    gpioInit.GPIO_Remap = GPIO_Remap_1;
    GPIO_Init(DS28S60_CS_PORT, &gpioInit);

    gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
    gpioInit.GPIO_Pin = DS28S60_PDWN_PIN;
    gpioInit.GPIO_Remap = GPIO_Remap_1;
    GPIO_Init(DS28S60_PDWN_PORT, &gpioInit);
    DS28S60_CS_SET;
    DS28S60_PDWN_SET;
}


static int32_t DS28S60_GetInfo(void)
{
    uint8_t data[32];
    int32_t ret;
    ret = DS28S60_ReadPage(data, ROM_OPTION_PG);
    if (ret != DS28S60_SUCCESS) {
        return ret;
    }
    if (CheckEntropy(&data[24], 8) == false) {
        return ERR_DS28S60_INFO;
    }
    memcpy(g_ds28s60Info.ROMID, data + 24, 8);
    memcpy(g_ds28s60Info.MANID, data + 22, 2);
    g_ds28s60Info.valid = DS28S60_INFO_VALID;
    return DS28S60_SUCCESS;
}


static void DS28S60_GetHmacKey(uint8_t *key, const DS28S60_Info_t *info, uint8_t pg, uint8_t cmd)
{
    uint8_t msg[76], masterSecret[32], bindingPageData[32], partialSecret[32];

    GetMasterSecret(masterSecret);
    GetBindingPageData(bindingPageData);
    GetPartialSecret(partialSecret);
    memcpy(&msg[0], info->ROMID, 8);
    memcpy(&msg[8], bindingPageData, 32);
    memcpy(&msg[40], partialSecret, 32);
    msg[72] = pg;
    memcpy(&msg[73], info->MANID, 2);
    msg[75] = cmd;

    GetMasterSecret(masterSecret);
    sha256_hmac((uint8_t *)masterSecret, 32, msg, 76, key);
    CLEAR_ARRAY(msg);
    CLEAR_ARRAY(masterSecret);
    CLEAR_ARRAY(bindingPageData);
    CLEAR_ARRAY(partialSecret);
}


//resultArray does not contain len-byte and result-byte.
static int32_t DS28S60_SendCmdAndGetResult(uint8_t cmd, uint8_t *para, uint8_t paraLen, uint8_t expectedLen, uint8_t *resultArray)
{
    uint32_t tryCount = 0;
    int32_t ret;
    while (tryCount++ < RETRY_MAX_COUNT) {
        ret = DS28S60_TrySendCmdAndGetResult(cmd, para, paraLen, expectedLen, resultArray);
        if (ret == DS28S60_SUCCESS) {
            break;
        }
        printf("retry %d\r\n", tryCount);
    }
    return ret;
}


static int32_t DS28S60_TrySendCmdAndGetResult(uint8_t cmd, uint8_t *para, uint8_t paraLen, uint8_t expectedLen, uint8_t *resultArray)
{
    uint8_t length, result;
    //uint32_t startTick;
    uint32_t tryCount;
    DS28S60_CS_CLR;

    SpiIoSendData(&DS28S60_SPI_CONFIG, &cmd, 1);
    SpiIoSendData(&DS28S60_SPI_CONFIG, &paraLen, 1);
    SpiIoSendData(&DS28S60_SPI_CONFIG, para, paraLen);
    //startTick = osKernelGetTickCount();
    tryCount = 0;
    do {
        UserDelay(WAIT_DELAY_TICK);
        SpiIoRecvData(&DS28S60_SPI_CONFIG, &length, 1);
        if (tryCount > DS28S60_OVERTIME / WAIT_DELAY_TICK) {
            printf("ds28s60 overtime, length=%d\r\n", length);
            DS28S60_CS_SET;
            return ERR_DS28S60_OVERTIME;
        }
        tryCount++;
    } while (length == 0 || length == 255);
    if (length - 1 != expectedLen && length != 1) {
        printf("length=%d,expectedLen=%d\r\n", length, expectedLen);
        DS28S60_CS_SET;
        return ERR_DS28S60_UNEXPECTLEN;
    }
    SpiIoRecvData(&DS28S60_SPI_CONFIG, &result, 1);
    if (resultArray != NULL) {
        SpiIoRecvData(&DS28S60_SPI_CONFIG, resultArray, length - 1);
    }
    //endTick = osKernelGetTickCount();
    DS28S60_CS_SET;
    //printf("used tick=%d\r\n", endTick - startTick);
    if (result == DS28S60_RESULT_BYTE_SUCCESS) {
        return DS28S60_SUCCESS;
    } else {
        printf("length=%d,expectedLen=%d\r\n", length, expectedLen);
        printf("result=%d\r\n", result);
        return result;
    }
}


int32_t DS28S60_ReadPage(uint8_t *data, uint8_t page)
{
    return DS28S60_SendCmdAndGetResult(DS28S60_CMD_READ_MEM, &page, 1, 32, data);
}


int32_t DS28S60_WritePage(uint8_t *data, uint8_t page)
{
    uint8_t sendBuf[33];
    sendBuf[0] = page;
    memcpy(&sendBuf[1], data, 32);
    return DS28S60_SendCmdAndGetResult(DS28S60_CMD_WRITE_MEM, sendBuf, 33, 0, NULL);
}


int32_t DS28S60_GetRng(uint8_t *rngArray, uint32_t num)
{
    int32_t ret;
    uint8_t buffer[253], copyNum;
    uint32_t i = 0;

    while (i < num) {
        copyNum = (num - i) > 253 ? 253 : (num - i);
        ret = DS28S60_SendCmdAndGetResult(DS28S60_CMD_READ_RNG, &copyNum, 1, copyNum, buffer);
        CHECK_ERRCODE_BREAK("ds28s60_getrng", ret);
        memcpy(rngArray + i, buffer, copyNum);
        i += copyNum;
    }
    CLEAR_ARRAY(buffer);
    return ret;
}


int32_t DS28S60_HmacAuthentication(uint8_t page)
{
    uint8_t hmacDevice[32];
    uint8_t hmacCalc[32];
    uint8_t challenge[34];
    //uint8_t pageData[32];
    uint8_t secretKey[32];
    uint8_t msg[76];
    int32_t ret;

    challenge[0] = page;
    challenge[1] = 0x00;            //Secret A
    TrngGet(&challenge[2], 32);
    if (g_ds28s60Info.valid == false) {
        ret = DS28S60_GetInfo();
        if (ret != DS28S60_SUCCESS) {
            return ret;
        }
    }
    ret = DS28S60_SendCmdAndGetResult(DS28S60_CMD_CRPA, challenge, 34, 32, hmacDevice);
    if (ret != DS28S60_SUCCESS) {
        return ret;
    }
    UserDelay(50);
    ret = DS28S60_ReadPage(&msg[8], page);
    if (ret != DS28S60_SUCCESS) {
        return ret;
    }
    UserDelay(50);
    DS28S60_GetHmacKey(secretKey, &g_ds28s60Info, BINDING_DATA_PAGE, DS28S60_CMD_CPT_SECRET);
    memcpy(&msg[0], g_ds28s60Info.ROMID, 8);
    memcpy(&msg[40], &challenge[2], 32);
    msg[72] = page;
    memcpy(&msg[73], g_ds28s60Info.MANID, 2);
    msg[75] = DS28S60_CMD_CRPA;
    //PrintArray("msg", msg, 76);
    sha256_hmac(secretKey, 32, msg, 76, hmacCalc);
    //PrintArray("hmacCalc", hmacCalc, 32);
    //PrintArray("hmacCalc", hmacDevice, 32);
    if (memcmp(hmacCalc, hmacDevice, 32) != 0) {
        return ERR_DS28S60_AUTH;
    }
    CLEAR_ARRAY(hmacDevice);
    CLEAR_ARRAY(hmacCalc);
    CLEAR_ARRAY(challenge);
    CLEAR_ARRAY(secretKey);
    CLEAR_ARRAY(msg);

    return DS28S60_SUCCESS;
}


int32_t DS28S60_HmacEncryptRead(uint8_t *data, uint8_t page)
{
    uint8_t buf[40];
    uint8_t msg[20];
    uint8_t secretKey[32];
    uint8_t hmac[32];
    uint32_t i;
    int32_t ret;

    do {
        if (g_ds28s60Info.valid == false) {
            ret = DS28S60_GetInfo();
            CHECK_ERRCODE_BREAK("DS28S60_GetInfo", ret);
        }

        ret = DS28S60_SendCmdAndGetResult(DS28S60_CMD_ENC_READ, &page, 1, 40, buf);
        CHECK_ERRCODE_BREAK("DS28S60_SendCmdAndGetResult", ret);
        memcpy(&msg[0], buf, 8);
        memcpy(&msg[8], g_ds28s60Info.ROMID, 8);
        msg[16] = page;
        memcpy(&msg[17], g_ds28s60Info.MANID, 2);
        msg[19] = DS28S60_CMD_ENC_READ;
        DS28S60_GetHmacKey(secretKey, &g_ds28s60Info, BINDING_DATA_PAGE, DS28S60_CMD_CPT_SECRET);
        sha256_hmac(secretKey, 32, msg, 20, hmac);
        for (i = 0; i < 32; i++) {
            data[i] = hmac[i] ^ buf[i + 8];
        }
    } while (0);

    CLEAR_ARRAY(buf);
    CLEAR_ARRAY(msg);
    CLEAR_ARRAY(secretKey);
    CLEAR_ARRAY(hmac);

    return ret;
}


int32_t DS28S60_HmacEncryptWrite(const uint8_t *data, uint8_t page)
{
    uint8_t oldData[32];
    uint8_t secretKey[32];
    uint8_t msg[76];
    uint8_t challenge[8];
    uint8_t hmac[32];
    uint8_t sendBuf[73];
    int32_t ret;
    uint32_t i;

    do {
        ret = DS28S60_HmacEncryptRead(oldData, page);
        CHECK_ERRCODE_BREAK("DS28S60_HmacEncryptRead", ret);
        sendBuf[0] = page;
        DS28S60_GetHmacKey(secretKey, &g_ds28s60Info, BINDING_DATA_PAGE, DS28S60_CMD_CPT_SECRET);
        //PrintArray("secretKey", secretKey, 32);
        TrngGet(challenge, 8);
        //PrintArray("challenge", challenge, 8);
        memcpy(&msg[0], challenge, 8);
        memcpy(&msg[8], g_ds28s60Info.ROMID, 8);
        msg[16] = page;
        memcpy(&msg[17], g_ds28s60Info.MANID, 2);
        msg[19] = DS28S60_CMD_AUTH_WRITE;
        sha256_hmac(secretKey, 32, msg, 20, hmac);
        //PrintArray("hmac", hmac, 32);
        for (i = 0; i < 32; i ++) {
            sendBuf[i + 1] = data[i] ^ hmac[i];
        }
        //PrintArray("encrypted data", sendBuf + 1, 32);
        memcpy(&msg[0], g_ds28s60Info.ROMID, 8);
        memcpy(&msg[8], oldData, 32);
        memcpy(&msg[40], data, 32);
        msg[72] = page;
        memcpy(&msg[73], g_ds28s60Info.MANID, 2);
        msg[75] = DS28S60_CMD_AUTH_WRITE;
        //PrintArray("msg", msg, 76);
        sha256_hmac(secretKey, 32, msg, 76, hmac);
        //PrintArray("hmac", hmac, 32);
        memcpy(&sendBuf[33], hmac, 32);
        memcpy(&sendBuf[65], challenge, 8);
        //PrintArray("sendBuf", sendBuf, 73);
        ret = DS28S60_SendCmdAndGetResult(DS28S60_CMD_AUTH_WRITE, sendBuf, 73, 0, NULL);
        CHECK_ERRCODE_BREAK("DS28S60_SendCmdAndGetResult", ret);
    } while (0);

    CLEAR_ARRAY(oldData);
    CLEAR_ARRAY(secretKey);
    CLEAR_ARRAY(msg);
    CLEAR_ARRAY(challenge);
    CLEAR_ARRAY(hmac);
    CLEAR_ARRAY(sendBuf);

    return ret;
}


/// @brief Get master secret from MCU OTP.
/// @param[out] masterSecret master secret, 32 bytes.
static void GetMasterSecret(uint8_t *masterSecret)
{
#ifdef DS28S60_TEST_MODE
    memcpy(masterSecret, MASTER_SECRET, sizeof(MASTER_SECRET));
#else
    OTP_PowerOn();
    memcpy(masterSecret, (uint8_t *)MASTER_SECRET_ADDR, 32);
#endif
}


/// @brief Get binding page data from MCU OTP.
/// @param[out] bindingPageData binding page data, 32 bytes.
static void GetBindingPageData(uint8_t *bindingPageData)
{
#ifdef DS28S60_TEST_MODE
    memcpy(bindingPageData, BINDING_PAGE_DATA, sizeof(BINDING_PAGE_DATA));
#else
    OTP_PowerOn();
    memcpy(bindingPageData, (uint8_t *)BINDING_PAGE_DATA_ADDR, 32);
#endif
}


/// @brief Get partial secret from MCU OTP.
/// @param[out] partialSecret partial secret, 32 bytes.
static void GetPartialSecret(uint8_t *partialSecret)
{
#ifdef DS28S60_TEST_MODE
    memcpy(partialSecret, PARTIAL_SECRET, sizeof(PARTIAL_SECRET));
#else
    OTP_PowerOn();
    memcpy(partialSecret, (uint8_t *)PARTIAL_SECRET_ADDR, 32);
#endif
}


/*
 * LgpioShim.c
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "LgpioShim.h"

#define MAX_MOCK_GPIOS 64
#define MAX_SPI_RECORD 256

static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

static int s_failGpioOpen = 0;
static int s_failSpiOpen = 0;
static int s_failSpiWrite = 0;

static int s_gpioChipOpen = 0;
static int s_spiOpen = 0;

static int s_gpioClaimed[MAX_MOCK_GPIOS];
static int s_gpioLevel[MAX_MOCK_GPIOS];
static int s_gpioWriteCount[MAX_MOCK_GPIOS];

static int s_spiWriteCount = 0;
static uint8_t s_lastSpiData[MAX_SPI_RECORD];
static size_t s_lastSpiLen = 0;

void lgShimReset(void)
{
    pthread_mutex_lock(&s_mutex);
    s_failGpioOpen = 0;
    s_failSpiOpen = 0;
    s_failSpiWrite = 0;
    s_gpioChipOpen = 0;
    s_spiOpen = 0;
    memset(s_gpioClaimed, 0, sizeof(s_gpioClaimed));
    memset(s_gpioLevel, 0, sizeof(s_gpioLevel));
    memset(s_gpioWriteCount, 0, sizeof(s_gpioWriteCount));
    s_spiWriteCount = 0;
    s_lastSpiLen = 0;
    memset(s_lastSpiData, 0, sizeof(s_lastSpiData));
    pthread_mutex_unlock(&s_mutex);
}

void lgShimSetFailGpioOpen(int fail)
{
    pthread_mutex_lock(&s_mutex);
    s_failGpioOpen = fail;
    pthread_mutex_unlock(&s_mutex);
}

void lgShimSetFailSpiOpen(int fail)
{
    pthread_mutex_lock(&s_mutex);
    s_failSpiOpen = fail;
    pthread_mutex_unlock(&s_mutex);
}

void lgShimSetFailSpiWrite(int fail)
{
    pthread_mutex_lock(&s_mutex);
    s_failSpiWrite = fail;
    pthread_mutex_unlock(&s_mutex);
}

int lgShimGetGpioClaimed(int gpio)
{
    int val = 0;
    pthread_mutex_lock(&s_mutex);
    if (gpio >= 0 && gpio < MAX_MOCK_GPIOS) {
        val = s_gpioClaimed[gpio];
    }
    pthread_mutex_unlock(&s_mutex);
    return val;
}

int lgShimGetGpioLevel(int gpio)
{
    int val = -1;
    pthread_mutex_lock(&s_mutex);
    if (gpio >= 0 && gpio < MAX_MOCK_GPIOS) {
        val = s_gpioLevel[gpio];
    }
    pthread_mutex_unlock(&s_mutex);
    return val;
}

int lgShimGetGpioWriteCount(int gpio)
{
    int val = 0;
    pthread_mutex_lock(&s_mutex);
    if (gpio >= 0 && gpio < MAX_MOCK_GPIOS) {
        val = s_gpioWriteCount[gpio];
    }
    pthread_mutex_unlock(&s_mutex);
    return val;
}

int lgShimGetSpiWriteCount(void)
{
    int val = 0;
    pthread_mutex_lock(&s_mutex);
    val = s_spiWriteCount;
    pthread_mutex_unlock(&s_mutex);
    return val;
}

size_t lgShimGetLastSpiData(void *buf, size_t maxLen)
{
    size_t copied = 0;
    pthread_mutex_lock(&s_mutex);
    if (buf != NULL && maxLen > 0) {
        copied = (s_lastSpiLen < maxLen) ? s_lastSpiLen : maxLen;
        memcpy(buf, s_lastSpiData, copied);
    }
    pthread_mutex_unlock(&s_mutex);
    return copied;
}

int lgGpiochipOpen(int gpioDev)
{
    (void) gpioDev;
    pthread_mutex_lock(&s_mutex);
    if (s_failGpioOpen) {
        pthread_mutex_unlock(&s_mutex);
        return LG_BAD_HANDLE;
    }
    s_gpioChipOpen++;
    pthread_mutex_unlock(&s_mutex);
    return 100;
}

int lgGpiochipClose(int handle)
{
    pthread_mutex_lock(&s_mutex);
    if (handle == 100 && s_gpioChipOpen > 0) {
        s_gpioChipOpen--;
    }
    pthread_mutex_unlock(&s_mutex);
    return LG_OKAY;
}

int lgGpioGetChipInfo(int handle, lgChipInfo_p chipInfo)
{
    if (handle != 100 || chipInfo == NULL) {
        return LG_BAD_HANDLE;
    }
    chipInfo->lines = MAX_MOCK_GPIOS;
    strncpy(chipInfo->name, "mock-gpiochip", sizeof(chipInfo->name) - 1);
    chipInfo->name[sizeof(chipInfo->name) - 1] = '\0';
    strncpy(chipInfo->label, "pinctrl-bcm2711", sizeof(chipInfo->label) - 1);
    chipInfo->label[sizeof(chipInfo->label) - 1] = '\0';
    return LG_OKAY;
}

int lgGpioClaimOutput(int handle, int lFlags, int gpio, int level)
{
    (void) lFlags;
    if (handle != 100 || gpio < 0 || gpio >= MAX_MOCK_GPIOS) {
        return LG_BAD_HANDLE;
    }
    pthread_mutex_lock(&s_mutex);
    s_gpioClaimed[gpio] = 1;
    s_gpioLevel[gpio] = level ? 1 : 0;
    pthread_mutex_unlock(&s_mutex);
    return LG_OKAY;
}

int lgGpioFree(int handle, int gpio)
{
    if (handle != 100 || gpio < 0 || gpio >= MAX_MOCK_GPIOS) {
        return LG_BAD_HANDLE;
    }
    pthread_mutex_lock(&s_mutex);
    s_gpioClaimed[gpio] = 0;
    pthread_mutex_unlock(&s_mutex);
    return LG_OKAY;
}

int lgGpioWrite(int handle, int gpio, int level)
{
    if (handle != 100 || gpio < 0 || gpio >= MAX_MOCK_GPIOS) {
        return LG_BAD_HANDLE;
    }
    pthread_mutex_lock(&s_mutex);
    s_gpioLevel[gpio] = level ? 1 : 0;
    s_gpioWriteCount[gpio]++;
    pthread_mutex_unlock(&s_mutex);
    return LG_OKAY;
}

int lgSpiOpen(int spiDev, int spiChan, int spiBaud, int spiFlags)
{
    (void) spiDev;
    (void) spiChan;
    (void) spiBaud;
    (void) spiFlags;
    pthread_mutex_lock(&s_mutex);
    if (s_failSpiOpen) {
        pthread_mutex_unlock(&s_mutex);
        return LG_BAD_HANDLE;
    }
    s_spiOpen++;
    pthread_mutex_unlock(&s_mutex);
    return 200;
}

int lgSpiClose(int handle)
{
    pthread_mutex_lock(&s_mutex);
    if (handle == 200 && s_spiOpen > 0) {
        s_spiOpen--;
    }
    pthread_mutex_unlock(&s_mutex);
    return LG_OKAY;
}

int lgSpiWrite(int handle, const char *txBuf, int count)
{
    if (handle != 200 || txBuf == NULL || count < 0) {
        return LG_BAD_HANDLE;
    }
    pthread_mutex_lock(&s_mutex);
    if (s_failSpiWrite) {
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }
    s_spiWriteCount++;
    if (count > 0) {
        size_t toCopy = (count > (int) sizeof(s_lastSpiData)) ?
            sizeof(s_lastSpiData) : (size_t) count;
        memcpy(s_lastSpiData, txBuf, toCopy);
        s_lastSpiLen = toCopy;
    }
    pthread_mutex_unlock(&s_mutex);
    return count;
}

const char *lguErrorText(int error)
{
    switch (error) {
    case LG_OKAY:
        return "LG_OKAY: success";
    case LG_BAD_HANDLE:
        return "LG_BAD_HANDLE: bad handle";
    case LG_BAD_GPIO_NUMBER:
        return "LG_BAD_GPIO_NUMBER: bad GPIO number";
    default:
        return "LG_ERROR: mock generic error";
    }
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

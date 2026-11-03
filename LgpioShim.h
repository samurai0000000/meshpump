/*
 * LgpioShim.h
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef LGPIOSHIM_H
#define LGPIOSHIM_H

#if !defined(_LGPIO_H) && !defined(LGPIO_H)
#define _LGPIO_H
#define LGPIO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LG_OKAY                   0
#define LG_BAD_HANDLE            -5
#define LG_BAD_GPIO_NUMBER      -73
#define LG_GPIO_LABEL_LEN        32
#define LG_GPIO_NAME_LEN         32

typedef struct lgChipInfo_s {
    uint32_t lines;
    char name[LG_GPIO_NAME_LEN];
    char label[LG_GPIO_LABEL_LEN];
} lgChipInfo_t, *lgChipInfo_p;

int lgGpiochipOpen(int gpioDev);
int lgGpiochipClose(int handle);
int lgGpioGetChipInfo(int handle, lgChipInfo_p chipInfo);
int lgGpioClaimOutput(int handle, int lFlags, int gpio, int level);
int lgGpioFree(int handle, int gpio);
int lgGpioWrite(int handle, int gpio, int level);

int lgSpiOpen(int spiDev, int spiChan, int spiBaud, int spiFlags);
int lgSpiClose(int handle);
int lgSpiWrite(int handle, const char *txBuf, int count);

const char *lguErrorText(int error);

#ifdef __cplusplus
}
#endif

#endif /* !defined(_LGPIO_H) && !defined(LGPIO_H) */

#ifdef __cplusplus
extern "C" {
#endif

/* Mock state inspection and fault-injection helpers */
void lgShimReset(void);
void lgShimSetFailGpioOpen(int fail);
void lgShimSetFailSpiOpen(int fail);
void lgShimSetFailSpiWrite(int fail);
int lgShimGetGpioClaimed(int gpio);
int lgShimGetGpioLevel(int gpio);
int lgShimGetGpioWriteCount(int gpio);
int lgShimGetSpiWriteCount(void);
size_t lgShimGetLastSpiData(void *buf, size_t maxLen);

#ifdef __cplusplus
}
#endif

#endif /* LGPIOSHIM_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

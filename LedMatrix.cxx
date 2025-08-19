/*
 * LedMatrix.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pigpiod_if.h>
#include <cstring>
#include <iostream>
#include "font8x8/font8x8.h"
#include <max7219_defs.h>
#include <LedMatrix.hxx>

#define MAX7219_SPI_CHAN   0
#define MAX7219_SPI_SPEED  1000000
#define MAX7219_SPI_MODE                    \
    PI_SPI_FLAGS_BITLEN(0)   |              \
    PI_SPI_FLAGS_RX_LSB(0)   |              \
    PI_SPI_FLAGS_TX_LSB(0)   |              \
    PI_SPI_FLAGS_3WREN(0)    |              \
    PI_SPI_FLAGS_3WIRE(0)    |              \
    PI_SPI_FLAGS_AUX_SPI(0)  |              \
    PI_SPI_FLAGS_RESVD(0)    |              \
    PI_SPI_FLAGS_CSPOLS(0)   |              \
    PI_SPI_FLAGS_MODE(0)
#define MAX7219_COUNT      (4 * 4)

/*
 * MAX 7219:
 * https://www.sparkfun.com/datasheets/Components/General/COM-09622-MAX7219-MAX7221.pdf
 *
 * yellow    din   mosi(10)
 * green     cs    spi_ce0(8)
 * blue      clk   spi_clk(11)
 */

LedMatrix::LedMatrix()
  : _intensity(1),
    _fb()
{
    _handle = spi_open(MAX7219_SPI_CHAN, MAX7219_SPI_SPEED, MAX7219_SPI_MODE);
    if (_handle < 0) {
        cerr << "spiOpen failed!" << endl;
        exit(EXIT_FAILURE);
    }

    writeMax7219(DECODE_MODE_REG, 0);
    writeMax7219(INTENSITY_REG, _intensity);
    writeMax7219(SCAN_LIMIT_REG, 7);
    writeMax7219(SHUTDOWN_REG, 1);
    writeMax7219(DISPLAY_TEST_REG, 0);
    writeMax7219(DIGIT0_REG, 0);
    writeMax7219(DIGIT1_REG, 0);
    writeMax7219(DIGIT2_REG, 0);
    writeMax7219(DIGIT3_REG, 0);
    writeMax7219(DIGIT4_REG, 0);
    writeMax7219(DIGIT5_REG, 0);
    writeMax7219(DIGIT6_REG, 0);
    writeMax7219(DIGIT7_REG, 0);

    setText(0, "");
    setText(1, "");
    setText(2, "");
    setText(3, "");

    _running = true;
    pthread_mutex_init(&_mutex, NULL);
    pthread_cond_init(&_cond, NULL);
    pthread_create(&_thread, NULL, LedMatrix::thread_func, this);
    pthread_setname_np(_thread, "R'LedMatrix");
}

LedMatrix::~LedMatrix()
{
    _running = false;
    pthread_cond_broadcast(&_cond);
    pthread_join(_thread, NULL);

    if (_handle >= 0) {
        spi_close(_handle);
        _handle = -1;
    }

    pthread_mutex_destroy(&_mutex);
    pthread_cond_destroy(&_cond);
}

void LedMatrix::stop(void)
{
    _running = false;
    pthread_cond_broadcast(&_cond);
}

void LedMatrix::join(void)
{
    pthread_join(_thread, NULL);
}

void *LedMatrix::thread_func(void *args)
{
    LedMatrix *matrix = (LedMatrix *) args;

    matrix->run();

    return NULL;
}

static struct timespec *timespecadd(struct timespec *result,
                                    struct timespec *ts1,
                                    struct timespec *ts2) {

    result->tv_nsec = ts1->tv_nsec + ts2->tv_nsec;
    result->tv_sec = ts1->tv_sec + ts2->tv_sec;

    if (result->tv_nsec >= 1000000000) {
        result->tv_sec += result->tv_nsec / 1000000000;
        result->tv_nsec %= 1000000000;
    }

    return result;
}

void LedMatrix::run(void)
{
    struct timespec ts, tloop;
    unsigned int cycle;
    unsigned int i;

    tloop.tv_sec = 0;
    tloop.tv_nsec = 250000000;

    for (cycle = 0; _running; cycle++) {
        clock_gettime(CLOCK_REALTIME, &ts);
        timespecadd(&ts, &tloop, &ts);

        for (i = 0; i < 4; i++) {
            if (((_pos[i] + 0) >= 0) &&
                ((_pos[i] + 0) < (int) _text[i].size())) {
                draw(i, 0, (const uint8_t *)
                     font8x8_basic[_text[i][_pos[i] + 0] % 128]);
            } else {
                draw(i, 0, (const uint8_t *) font8x8_basic[0]);
            }
            if (((_pos[i] + 1) >= 0) &&
                (_pos[i] + 1) < (int) _text[i].size()) {
                draw(i, 1, (const uint8_t *)
                     font8x8_basic[_text[i][_pos[i] + 1] % 128]);
            } else {
                draw(i, 1, (const uint8_t *) font8x8_basic[0]);
            }
            if (((_pos[i] + 2) >= 0) &&
                (_pos[i] + 2) < (int) _text[i].size()) {
                draw(i, 2, (const uint8_t *)
                     font8x8_basic[_text[i][_pos[i] + 2] % 128]);
            } else {
                draw(i, 2, (const uint8_t *) font8x8_basic[0]);
            }
            if (((_pos[i] + 3) >= 0) &&
                (_pos[i] + 3) < (int) _text[i].size()) {
                draw(i, 3, (const uint8_t *)
                     font8x8_basic[_text[i][_pos[i] + 3] % 128]);
            } else {
                draw(i, 3, (const uint8_t *) font8x8_basic[0]);
            }

            if (_text[i].size() > 4) {
                _pos[i]++;
            }

            if (_pos[i] > (int) _text[i].size()) {
                _pos[i] = -4;
            }
        }

        repaint();

        pthread_mutex_lock(&_mutex);
        pthread_cond_timedwait(&_cond, &_mutex, &ts);
        pthread_mutex_unlock(&_mutex);
    }

    writeMax7219(SHUTDOWN_REG, 0);
}

int LedMatrix::writeMax7219(const void *data, size_t size)
{
    int ret;

    pthread_mutex_lock(&_mutex);
    ret = spi_write(_handle, (char *) data, size);
    pthread_mutex_unlock(&_mutex);
    if (ret != (int) size) {
        cerr << "spi_write failed!" << endl;
    }

    return ret;
}

int LedMatrix::writeMax7219(uint8_t reg, uint8_t data)
{
    int ret;
    unsigned int i;
    char xmit[MAX7219_COUNT * 2];

    for (i = 0; i < MAX7219_COUNT; i++) {
        xmit[i * 2 + 0] = reg;
        xmit[i * 2 + 1] = data;
    }

    pthread_mutex_lock(&_mutex);
    ret = spi_write(_handle, xmit, sizeof(xmit));
    pthread_mutex_unlock(&_mutex);
    if (ret != sizeof(xmit)) {
        cerr << "spi_write failed!" << endl;
    }

    return ret;
}

unsigned int LedMatrix::intensity(void) const
{
    return _intensity;
}

void LedMatrix::setIntensity(unsigned int intensity)
{
    _intensity = intensity;
    writeMax7219(INTENSITY_REG, _intensity);
}

void LedMatrix::clear(void)
{
    pthread_mutex_lock(&_mutex);
    _text[0].clear();
    _text[1].clear();
    _text[2].clear();
    _text[3].clear();
    bzero(_fb, sizeof(_fb));
    pthread_mutex_unlock(&_mutex);
}

void LedMatrix::setText(unsigned int layer, const string &text)
{
    pthread_mutex_lock(&_mutex);
    _text[layer] = text;
    if (_welcome[layer].empty()) {
        _welcome[layer] = text;
    }
    if (text.size() <= 4) {
        _pos[layer] = 0;
    } else {
        _pos[layer] = -4;
    }
    pthread_mutex_unlock(&_mutex);
}

void LedMatrix::setWelcomeText(void)
{
    setText(0, _welcome[0]);
    setText(1, _welcome[1]);
    setText(2, _welcome[2]);
    setText(3, _welcome[3]);
}

void LedMatrix::draw(unsigned int layer, const uint32_t fb[8])
{
    pthread_mutex_lock(&_mutex);
    for (unsigned int i = 0; i < 8; i++) {
        _fb[layer][i] = fb[i];
    }
    pthread_mutex_unlock(&_mutex);
}

void LedMatrix::draw(unsigned int i, unsigned int j, const uint8_t fb[8])
{
    uint8_t *one = ((uint8_t *) _fb[i]);

    pthread_mutex_lock(&_mutex);
    for (unsigned int k = 0; k < 8; k++) {
        one[k * 4 + j] = fb[k];
    }
    pthread_mutex_unlock(&_mutex);
}

void LedMatrix::repaint(void)
{
    uint8_t xmit[MAX7219_COUNT * 2];

    for (unsigned int i = 0; i < 8; i++) {
        xmit[ 0] = DIGIT7_REG - i;
        xmit[ 1] = (uint8_t) (_fb[0][i] >> 24);
        xmit[ 2] = DIGIT7_REG - i;
        xmit[ 3] = (uint8_t) (_fb[0][i] >> 16);
        xmit[ 4] = DIGIT7_REG - i;
        xmit[ 5] = (uint8_t) (_fb[0][i] >> 8);
        xmit[ 6] = DIGIT7_REG - i;
        xmit[ 7] = (uint8_t) (_fb[0][i] >> 0);
        xmit[ 8] = DIGIT7_REG - i;
        xmit[ 9] = (uint8_t) (_fb[1][i] >> 24);
        xmit[10] = DIGIT7_REG - i;
        xmit[11] = (uint8_t) (_fb[1][i] >> 16);
        xmit[12] = DIGIT7_REG - i;
        xmit[13] = (uint8_t) (_fb[1][i] >> 8);
        xmit[14] = DIGIT7_REG - i;
        xmit[15] = (uint8_t) (_fb[1][i] >> 0);
        xmit[16] = DIGIT7_REG - i;
        xmit[17] = (uint8_t) (_fb[2][i] >> 24);
        xmit[18] = DIGIT7_REG - i;
        xmit[19] = (uint8_t) (_fb[2][i] >> 16);
        xmit[20] = DIGIT7_REG - i;
        xmit[21] = (uint8_t) (_fb[2][i] >> 8);
        xmit[22] = DIGIT7_REG - i;
        xmit[23] = (uint8_t) (_fb[2][i] >> 0);
        xmit[24] = DIGIT7_REG - i;
        xmit[25] = (uint8_t) (_fb[3][i] >> 24);
        xmit[26] = DIGIT7_REG - i;
        xmit[27] = (uint8_t) (_fb[3][i] >> 16);
        xmit[28] = DIGIT7_REG - i;
        xmit[29] = (uint8_t) (_fb[3][i] >> 8);
        xmit[30] = DIGIT7_REG - i;
        xmit[31] = (uint8_t) (_fb[3][i] >> 0);

        writeMax7219(xmit, sizeof(xmit));
    }
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

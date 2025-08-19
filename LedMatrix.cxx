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
}

LedMatrix::~LedMatrix()
{
    stop();

    if (_handle >= 0) {
        spi_close(_handle);
        _handle = -1;
    }
}

void LedMatrix::start(void)
{
    if (_thread == NULL) {
        _running = true;
        _thread = make_shared<thread>(LedMatrix::thread_func, this);
    }
}

void LedMatrix::stop(void)
{
    _running = false;
}

void LedMatrix::join(void)
{
    if (_thread != NULL) {
        if (_thread->joinable()) {
            _thread->join();
        }
    }
}

void *LedMatrix::thread_func(void *args)
{
    LedMatrix *matrix = (LedMatrix *) args;

    matrix->run();

    return NULL;
}

void LedMatrix::run(void)
{
    unsigned int cycle;
    int x, y;

    for (cycle = 0; _running; cycle++) {

        for (y = 0; y < MAX7219_Y_COUNT; y++) {
            for (x = 0; x < MAX7219_X_COUNT; x++) {
                if (((_pos[y] + x) >= 0) &&
                    ((_pos[y] + x) < (int) _text[y].size())) {
                    draw(y, x, (const uint8_t *)
                         font8x8_basic[_text[y][_pos[y] + x] % 128]);
                } else {
                    draw(y, x, (const uint8_t *) font8x8_basic[0]);
                }
            }

            if (_text[y].size() > MAX7219_X_COUNT) {
                _pos[y]++;
            }

            if (_pos[y] > (int) _text[y].size()) {
                _pos[y] = -MAX7219_X_COUNT;
            }
        }

        repaint();
        usleep(250000);
    }

    writeMax7219(SHUTDOWN_REG, 0);
}

int LedMatrix::writeMax7219(const void *data, size_t size)
{
    int ret;

    ret = spi_write(_handle, (char *) data, size);
    if (ret != (int) size) {
        cerr << "spi_write failed!" << endl;
    }

    return ret;
}

int LedMatrix::writeMax7219(uint8_t reg, uint8_t data)
{
    int ret;
    unsigned int i;
    char xmit[MAX7219_X_COUNT * MAX7219_Y_COUNT * 2];

    for (i = 0; i < (MAX7219_X_COUNT * MAX7219_Y_COUNT); i++) {
        xmit[i * 2 + 0] = reg;
        xmit[i * 2 + 1] = data;
    }

    ret = spi_write(_handle, xmit, sizeof(xmit));
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
    _mutex.lock();
    for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
        _text[y].clear();
    }
    bzero(_fb, sizeof(_fb));
    _mutex.unlock();
}

void LedMatrix::setText(unsigned int y, const string &text)
{
    if (y >= MAX7219_Y_COUNT) {
        return;
    }

    _mutex.lock();
    _text[y] = text;
    if (_welcome[y].empty()) {
        _welcome[y] = text;
    }
    if (text.size() <= MAX7219_X_COUNT) {
        _pos[y] = 0;
    } else {
        _pos[y] = -MAX7219_X_COUNT;
    }
    _mutex.unlock();
}

void LedMatrix::setWelcomeText(void)
{
    for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
        setText(y, _welcome[y]);
    }
}

void LedMatrix::draw(unsigned int y, unsigned int x, const uint8_t fb[8])
{
    if (y >= MAX7219_Y_COUNT) {
        return;
    }

    if (x >= MAX7219_X_COUNT) {
        return;
    }

    if (fb == NULL) {
        return;
    }

    _mutex.lock();
    for (unsigned int i = 0; i < 8; i++) {
        _fb[y][x][i] = fb[i];
    }
    _mutex.unlock();
}

void LedMatrix::repaint(void)
{
    uint8_t xmit[MAX7219_X_COUNT * MAX7219_Y_COUNT * 2];

    _mutex.lock();
    for (unsigned int i = 0; i < 8; i++) {
        for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
            for (unsigned int x = 0; x < MAX7219_X_COUNT; x++) {
                xmit[(((y * MAX7219_Y_COUNT) + x) * 2) + 0] = DIGIT7_REG - i;
                xmit[(((y * MAX7219_Y_COUNT) + x) * 2) + 1] =
                    _fb[y][MAX7219_X_COUNT - 1 - x][i];
            }
        }

        writeMax7219(xmit, sizeof(xmit));
    }
    _mutex.unlock();
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

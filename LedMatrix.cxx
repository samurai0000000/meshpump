/*
 * LedMatrix.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <lgpio.h>
#include <climits>
#include <cstring>
#include <iostream>
#include <font8x8/font8x8_basic.h>
#include <max7219_defs.h>
#include <LedMatrix.hxx>

#define MAX7219_SPI_DEV    0
#define MAX7219_SPI_CHAN   0
#define MAX7219_SPI_SPEED  1000000

LedMatrix::LedMatrix()
    : _handle(-1),
      _intensity(1),
      _fb(),
      _running(false),
      _thread(NULL),
      _delay(25)
{
    for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
        _ttl[y] = 0;
        _pos[y] = 0;
        _slice[y] = 0;
        _counter[y] = y + 1;
        _reload[y] = y + 1;
    }

    _handle = lgSpiOpen(MAX7219_SPI_DEV, MAX7219_SPI_CHAN,
                        MAX7219_SPI_SPEED, 0);
    if (_handle < 0) {
        cerr << "lgSpiOpen failed: " << lguErrorText(_handle) << endl;
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

    for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
        setText(y, "", 0);
        setSlowdownFactor(y, y + 1);
    }
}

LedMatrix::~LedMatrix()
{
    stop();
    join();

    if (_handle >= 0) {
        lgSpiClose(_handle);
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
        _thread = NULL;
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
    int x, y, slice;
    int i;
    char cc, nc;
    const uint8_t *cl, *nl;
    bool scroll;
    time_t tlast, tnow;

    tlast = tnow = time(NULL);

    while (_running) {
        unsigned int delayMs = 25;

        {
            lock_guard<mutex> lock(_mutex);

            for (y = 0; y < MAX7219_Y_COUNT; y++) {

                slice = _slice[y];

                if (_text[y].size() <= MAX7219_X_COUNT) {
                    scroll = false;
                } else {
                    scroll = true;
                }

                for (x = 0; x < MAX7219_X_COUNT; x++) {
                    i = _pos[y] + x;

                    if ((i >= 0) && (i < (int) _text[y].size())) {
                        cc = _text[y][i];
                    } else {
                        cc = ' ';
                    }

                    if ((i >= -1) && (i < (int) (_text[y].size() - 1))) {
                        nc = _text[y][i + 1];
                    } else {
                        nc = ' ';
                    }

                    unsigned char ucc = static_cast<unsigned char>(cc);
                    unsigned char unc = static_cast<unsigned char>(nc);

                    if (!scroll) {
                        cl = (const uint8_t *) font8x8_basic[ucc % 128];
                        drawLocked(y, x, cl);
                    } else {
                        cl = (const uint8_t *) font8x8_basic[ucc % 128];
                        nl = (const uint8_t *) font8x8_basic[unc % 128];

                        drawSRLocked(y, x, slice, true, cl);
                        drawSLLocked(y, x, 8 - slice, false, nl);
                    }
                }

                _counter[y]--;

                if (_counter[y] == 0) {
                    _counter[y] = _reload[y];
                    _slice[y]++;
                }

                if (slice == 7) {
                    _slice[y] = 0;

                    if (_text[y].size() > MAX7219_X_COUNT) {
                        _pos[y]++;
                    }

                    if (_pos[y] > (int) _text[y].size()) {
                        _pos[y] = -MAX7219_X_COUNT;
                    }
                }
            }

            tnow = time(NULL);
            if (tnow != tlast) {
                tlast = tnow;
                for (y = 0; y < MAX7219_Y_COUNT; y++) {
                    if (_ttl[y] > 0) {
                        _ttl[y]--;
                        if (_ttl[y] == 0) {
                            setTextLocked(y, _welcome[y], 0);
                        }
                    }
                }
            }

            repaintLocked();
            delayMs = _delay;
        }

        usleep(delayMs * 1000);
    }

    writeMax7219(SHUTDOWN_REG, 0);
}

int LedMatrix::writeMax7219(const void *data, size_t size)
{
    int ret;

    if (_handle < 0) {
        return -1;
    }

    ret = lgSpiWrite(_handle, (const char *) data, (int) size);
    if (ret != (int) size) {
        cerr << "lgSpiWrite failed!" << endl;
    }

    return ret;
}

int LedMatrix::writeMax7219(uint8_t reg, uint8_t data)
{
    int ret;
    unsigned int i;
    char xmit[MAX7219_X_COUNT * MAX7219_Y_COUNT * 2];

    if (_handle < 0) {
        return -1;
    }

    for (i = 0; i < (MAX7219_X_COUNT * MAX7219_Y_COUNT); i++) {
        xmit[i * 2 + 0] = reg;
        xmit[i * 2 + 1] = data;
    }

    ret = lgSpiWrite(_handle, xmit, (int) sizeof(xmit));
    if (ret != (int) sizeof(xmit)) {
        cerr << "lgSpiWrite failed!" << endl;
    }

    return ret;
}

unsigned int LedMatrix::intensity(void) const
{
    lock_guard<mutex> lock(_mutex);
    return _intensity;
}

void LedMatrix::setIntensity(unsigned int intensity)
{
    lock_guard<mutex> lock(_mutex);
    _intensity = intensity;
    writeMax7219(INTENSITY_REG, _intensity);
}

void LedMatrix::clear(void)
{
    lock_guard<mutex> lock(_mutex);
    for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
        setTextLocked(y, "", 30);
    }
}

void LedMatrix::setText(unsigned int y, const string &text,
                        unsigned int ttl)
{
    if (y >= MAX7219_Y_COUNT) {
        return;
    }

    lock_guard<mutex> lock(_mutex);
    setTextLocked(y, text, ttl);
}

void LedMatrix::setTextLocked(unsigned int y, const string &text,
                              unsigned int ttl)
{
    _text[y] = text;
    _ttl[y] = ttl;
    if (ttl == 0 && _welcome[y].empty()) {
        _welcome[y] = text;
    }
    if (text.size() <= MAX7219_X_COUNT) {
        _pos[y] = 0;
        _slice[y] = 0;
    } else {
        _pos[y] = -MAX7219_X_COUNT;
        _slice[y] = 0;
    }
}

void LedMatrix::setWelcomeText(void)
{
    lock_guard<mutex> lock(_mutex);
    for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
        setTextLocked(y, _welcome[y], 0);
    }
}

void LedMatrix::setWelcomeText(unsigned int y, const string &text, bool apply)
{
    if (y >= MAX7219_Y_COUNT) {
        return;
    }

    lock_guard<mutex> lock(_mutex);
    _welcome[y] = text;
    if (apply) {
        setTextLocked(y, text, 0);
    }
}

unsigned int LedMatrix::ttl(unsigned int y) const
{
    if (y >= MAX7219_Y_COUNT) {
        return 0;
    }

    lock_guard<mutex> lock(_mutex);
    return _ttl[y];
}

void LedMatrix::setDelay(unsigned int ms)
{
    lock_guard<mutex> lock(_mutex);
    _delay = ms;
}

unsigned int LedMatrix::delay(void) const
{
    lock_guard<mutex> lock(_mutex);
    return _delay;
}

void LedMatrix::setSlowdownFactor(unsigned int y, unsigned int sf)
{
    if (y >= MAX7219_Y_COUNT) {
        return;
    }

    if (sf < 1) {
        sf = 1;
    }

    lock_guard<mutex> lock(_mutex);
    _reload[y] = sf;
    _counter[y] = sf;
}

unsigned int LedMatrix::slowdownFactor(unsigned int y) const
{
    if (y >= MAX7219_Y_COUNT) {
        return 1;
    }

    lock_guard<mutex> lock(_mutex);
    return _reload[y];
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

    lock_guard<mutex> lock(_mutex);
    drawLocked(y, x, fb);
}

void LedMatrix::drawLocked(unsigned int y, unsigned int x, const uint8_t fb[8])
{
    for (unsigned int i = 0; i < 8; i++) {
        _fb[y][x][i] = fb[i];
    }
}

void LedMatrix::drawSL(unsigned int y, unsigned int x,
                       unsigned int shift, bool clear,
                       const uint8_t fb[8])
{
    if (y >= MAX7219_Y_COUNT) {
        return;
    }

    if (x >= MAX7219_X_COUNT) {
        return;
    }

    if (shift >= 8) {
        return;
    }

    if (fb == NULL) {
        return;
    }

    lock_guard<mutex> lock(_mutex);
    drawSLLocked(y, x, shift, clear, fb);
}

void LedMatrix::drawSLLocked(unsigned int y, unsigned int x,
                             unsigned int shift, bool clear,
                             const uint8_t fb[8])
{
    for (unsigned int i = 0; i < 8; i++) {
        if (clear) {
            _fb[y][x][i] = (fb[i] << shift);
        } else {
            _fb[y][x][i] |= (fb[i] << shift);
        }
    }
}

void LedMatrix::drawSR(unsigned int y, unsigned int x,
                       unsigned int shift, bool clear,
                       const uint8_t fb[8])
{
    if (y >= MAX7219_Y_COUNT) {
        return;
    }

    if (x >= MAX7219_X_COUNT) {
        return;
    }

    if (shift >= 8) {
        return;
    }

    if (fb == NULL) {
        return;
    }

    lock_guard<mutex> lock(_mutex);
    drawSRLocked(y, x, shift, clear, fb);
}

void LedMatrix::drawSRLocked(unsigned int y, unsigned int x,
                             unsigned int shift, bool clear,
                             const uint8_t fb[8])
{
    for (unsigned int i = 0; i < 8; i++) {
        if (clear) {
            _fb[y][x][i] = (fb[i] >> shift);
        } else {
            _fb[y][x][i] |= (fb[i] >> shift);
        }
    }
}

void LedMatrix::repaint(void)
{
    lock_guard<mutex> lock(_mutex);
    repaintLocked();
}

void LedMatrix::repaintLocked(void)
{
    uint8_t xmit[MAX7219_X_COUNT * MAX7219_Y_COUNT * 2];

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

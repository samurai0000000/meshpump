/*
 * LedMatrix.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#if defined(USE_PIGPIO)
#include <pigpiod_if.h>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#endif
#include <cstring>
#include <iostream>
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
#define MAX7219_COUNT      4

#define ALWAYS_REFRESH_ALL

/*
 * MAX 7219:
 * https://www.sparkfun.com/datasheets/Components/General/COM-09622-MAX7219-MAX7221.pdf
 *
 * yellow    din   mosi(10)
 * green     cs    spi_ce0(8)
 * blue      clk   spi_clk(11)
 */

enum mouth_mode {
    MOUTH_BEH,
    MOUTH_SMILE,
    MOUTH_CYLON,
    MOUTH_SPEAK,
    MOUTH_TEXT,
};

LedMatrix::LedMatrix()
  : _intensity(1),
    _fb(),
    _mode(MOUTH_BEH)
{
#if defined(USE_PIGPIO)
    _handle = spi_open(MAX7219_SPI_CHAN, MAX7219_SPI_SPEED, MAX7219_SPI_MODE);
    if (_handle < 0) {
        cerr << "spiOpen failed!" << endl;
        exit(EXIT_FAILURE);
    }
#else
    int ret;
    uint8_t mode = SPI_MODE_0;
    uint8_t bits_per_word = 8;
    uint32_t speed_hz = MAX7219_SPI_SPEED;

    _fd = open("/dev/spidev0.0", O_RDWR);
    if (_fd == -1) {
        cerr << "/dev/spidev/0.0: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    ret = ioctl(_fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1) {
        cerr << "SPI_IOC_WR_MODE: " << strerror(errno) << endl;
        close(_fd);
        _fd = -1;
        exit(EXIT_FAILURE);
    }

    ret = ioctl(_fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word);
    if (ret == -1) {
        cerr << "SPI_IOC_WR_BITS_PER_WORD: " << strerror(errno) << endl;
        close(_fd);
        _fd = -1;
        exit(EXIT_FAILURE);
    }

    ret = ioctl(_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz);
    if (ret == -1) {
        cerr << "SPI_IOC_WR_MAX_SPEED_HZ: " << strerror(errno) << endl;
        close(_fd);
        _fd = -1;
        exit(EXIT_FAILURE);
    }
#endif

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

    beh();

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

#if defined(USE_PIGPIO)
    if (_handle >= 0) {
        spi_close(_handle);
        _handle = -1;
    }
#else
    if (_fd != -1) {
        close(_fd);
        _fd = -1;
    }
#endif

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

void LedMatrix::behCycle(unsigned int cycle)
{
    static const uint32_t bitmap[][8] = {
        {
            0x00000000,
            0x00000000,
            0x00000000,
            0x0007e000,
            0x03ffffc0,
            0x0007e000,
            0x00000000,
            0x00000000,
        }, {
            0x00000000,
            0x00000000,
            0x00000000,
            0x0003c000,
            0x07ffffe0,
            0x0003c000,
            0x00000000,
            0x00000000,
        }, {
            0x00000000,
            0x00000000,
            0x00000000,
            0x00018000,
            0x0ffffff0,
            0x00018000,
            0x00000000,
            0x00000000,
        }, {
            0x00000000,
            0x00000000,
            0x00000000,
            0x00000000,
            0x1ffffff8,
            0x00000000,
            0x00000000,
            0x00000000,
        },
    };
    static int x = 0, dir = 0;

    if ((cycle % 20) == 0) {
        draw(&bitmap[x][0]);

        if (dir == 0) {
            x++;
        } else {
            x--;
        }

        if (x >= (int) ((sizeof(bitmap) / 32) - 1)) {
            x = (sizeof(bitmap) / 32) - 1;
            dir = 1;
        } else if (x < 0) {
            x = 1;
            dir = 0;
        }
    }
}

void LedMatrix::smileCycle(unsigned int cycle)
{
    static const uint32_t bitmap[][8] = {
        {
            0xc0000003,
            0xc0000003,
            0x3000000c,
            0x3000000c,
            0x0c000030,
            0x0c000030,
            0x03ffffc0,
            0x03ffffc0,
        }, {
            0x00000000,
            0xc0000003,
            0x3000000c,
            0x3000000c,
            0x0c000030,
            0x0c000030,
            0x03ffffc0,
            0x03ffffc0,
        }, {
            0x00000000,
            0x00000000,
            0x00000000,
            0x3000000c,
            0x0c000030,
            0x0c000030,
            0x03ffffc0,
            0x03ffffc0,
        }, {
            0x00000000,
            0x00000000,
            0x00000000,
            0x00000000,
            0x0c000030,
            0x0c000030,
            0x03ffffc0,
            0x03ffffc0,
        }, {
            0x00000000,
            0x00000000,
            0x00000000,
            0x00000000,
            0x00000000,
            0x0c000030,
            0x03ffffc0,
            0x03ffffc0,
        }, {
            0x00000000,
            0x00000000,
            0x00000000,
            0x00000000,
            0x00000000,
            0x00000000,
            0x03ffffc0,
            0x03ffffc0,
        },
    };
    static int x = 0, dir = 0;

    if ((cycle % 20) == 0) {
        draw(&bitmap[x][0]);

        if (dir == 0) {
            x++;
        } else {
            x--;
        }

        if (x >= (int) ((sizeof(bitmap) / 32) - 1)) {
            x = (sizeof(bitmap) / 32) - 1;
            dir = 1;
        } else if (x < 0) {
            x = 0;
            dir = 0;
        }
    }
}

void LedMatrix::cylonCycle(unsigned int cycle)
{
    static int x = 0, dir = 0;
    unsigned int i;


    if (cycle) {
        for (i = 0; i < 8; i++) {
            _fb[i] = (0x1 << x);
        }

        if (dir == 0) {
            x++;
        } else {
            x--;
        }

        if (x > 31) {
            x = 31;
            dir = 1;
        } else if (x < 0) {
            x = 0;
            dir = 0;
        }
    }
}

void LedMatrix::speakCycle(unsigned int cycle)
{
    static const uint32_t bitmap[][8] = {
        {
            0x00000000,
            0x3ffffffc,
            0x3000000c,
            0x3000000c,
            0x3000000c,
            0x3000000c,
            0x3ffffffc,
            0x00000000,
        }, {
            0x00000000,
            0x00000000,
            0x3ffffffc,
            0x3000000c,
            0x3000000c,
            0x3000000c,
            0x3ffffffc,
            0x00000000,
        }, {
            0x00000000,
            0x00000000,
            0x3ffffffc,
            0x3000000c,
            0x3000000c,
            0x3ffffffc,
            0x00000000,
            0x00000000,
        }, {
            0x00000000,
            0x00000000,
            0x00000000,
            0x3ffffffc,
            0x3000000c,
            0x3ffffffc,
            0x00000000,
            0x00000000,
        }, {
            0x00000000,
            0x00000000,
            0x00000000,
            0x3ffffffc,
            0x3ffffffc,
            0x00000000,
            0x00000000,
            0x00000000,
        },
    };
    static int x = 0, dir = 0;

    if ((cycle % 5) == 0) {
        draw(&bitmap[x][0]);

        if (dir == 0) {
            x++;
        } else {
            x--;
        }

        if (x >= (int) ((sizeof(bitmap) / 32) - 1)) {
            x = (sizeof(bitmap) / 32) - 1;
            dir = 1;
        } else if (x < 0) {
            x = 0;
            dir = 0;
        }
    }
}

void LedMatrix::textCycle(unsigned int cycle)
{
    (void)(cycle);
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
    unsigned int l_intensity;
    uint32_t l_fb[8];
    char xmit[MAX7219_COUNT * 2];
    unsigned int cycle;

    l_intensity = _intensity;
    memset(l_fb, 0x0, sizeof(l_fb));

    tloop.tv_sec = 0;
    tloop.tv_nsec = 20000000;

    for (cycle = 0; _running; cycle++) {
        int ret;
        unsigned int i;

        clock_gettime(CLOCK_REALTIME, &ts);
        timespecadd(&ts, &tloop, &ts);

        switch (_mode) {
        case MOUTH_BEH:
            behCycle(cycle);
            break;
        case MOUTH_SMILE:
            smileCycle(cycle);
            break;
        case MOUTH_CYLON:
            cylonCycle(cycle);
            break;
        case MOUTH_SPEAK:
            speakCycle(cycle);
            break;
        case MOUTH_TEXT:
            textCycle(cycle);
            break;
        default:
            break;
        }

        /* Periodically reprogram the registers */
        if (cycle % 256 == 0) {
            writeMax7219(DECODE_MODE_REG, 0);
            writeMax7219(INTENSITY_REG, _intensity);
            writeMax7219(SCAN_LIMIT_REG, 7);
            writeMax7219(SHUTDOWN_REG, 1);
            writeMax7219(DISPLAY_TEST_REG, 0);
#if !defined(ALWAYS_REFRESH_ALL)
            for (i = 0; i < 8; i++) {
                l_fb[i] = ~_fb[i];  // Flip bits to force refresh
            }
#endif
        }

        /* Update intensity */
        if (l_intensity != _intensity) {
            l_intensity = _intensity;
            writeMax7219(INTENSITY_REG, l_intensity);
        }

#if defined(ALWAYS_REFRESH_ALL)
            for (i = 0; i < 8; i++) {
                l_fb[i] = ~_fb[i];  // Flip bits to force refresh
            }
#endif

        /* Update frame buffer */
        for (i = 0; i < 8; i++) {
            if (l_fb[i] != _fb[i]) {
                l_fb[i] = _fb[i];
                xmit[0] = DIGIT7_REG - i;
                xmit[1] = (uint8_t) (l_fb[i] >> 24);
                xmit[2] = DIGIT7_REG - i;
                xmit[3] = (uint8_t) (l_fb[i] >> 16);
                xmit[4] = DIGIT7_REG - i;
                xmit[5] = (uint8_t) (l_fb[i] >> 8);
                xmit[6] = DIGIT7_REG - i;
                xmit[7] = (uint8_t) (l_fb[i] >> 0);

                ret = writeMax7219(xmit, sizeof(xmit));
                (void)(ret);
            }
        }

        pthread_mutex_lock(&_mutex);
        pthread_cond_timedwait(&_cond, &_mutex, &ts);
        pthread_mutex_unlock(&_mutex);
    }

    writeMax7219(SHUTDOWN_REG, 0);
}

int LedMatrix::writeMax7219(const void *data, size_t size)
{
    int ret;

#if defined(USE_PIGPIO)
    ret = spi_write(_handle, (char *) data, size);
    if (ret != (int) size) {
        cerr << "spi_write failed!" << endl;
    }
#else
    struct spi_ioc_transfer xfer;

    bzero(&xfer, sizeof(xfer));
    xfer.tx_buf = (__u64) data;
    xfer.len = size;
    xfer.bits_per_word = 8;

    ret = ioctl(_fd, SPI_IOC_MESSAGE(1), &xfer);
    if (ret == -1) {
        cerr << "SPI_IOC_MESSAGE: " << strerror(errno) << endl;
    }
#endif

    return ret;
}

int LedMatrix::writeMax7219(uint8_t reg, uint8_t data) const
{
    int ret;
    unsigned int i;
    char xmit[MAX7219_COUNT * 2];

    for (i = 0; i < MAX7219_COUNT; i++) {
        xmit[i * 2 + 0] = reg;
        xmit[i * 2 + 1] = data;
    }

#if defined(USE_PIGPIO)
    ret = spi_write(_handle, xmit, sizeof(xmit));
    if (ret != sizeof(xmit)) {
        cerr << "spi_write failed!" << endl;
    }
#else
    struct spi_ioc_transfer xfer;

    bzero(&xfer, sizeof(xfer));
    xfer.tx_buf = (__u64) xmit;
    xfer.len = sizeof(xmit);
    xfer.bits_per_word = 8;

    ret = ioctl(_fd, SPI_IOC_MESSAGE(1), &xfer);
    if (ret == -1) {
        cerr << "SPI_IOC_MESSAGE: " << strerror(errno) << endl;
    }
#endif

    return ret;
}

unsigned int LedMatrix::intensity(void) const
{
    return _intensity;
}

void LedMatrix::setIntensity(unsigned int intensity)
{
    _intensity = intensity;
}

void LedMatrix::draw(const uint32_t fb[8])
{
    memcpy(_fb, fb, sizeof(_fb));
}

unsigned int LedMatrix::mode(void) const
{
    return _mode;
}

void LedMatrix::setMode(unsigned int mode)
{
    _mode = mode;
}

void LedMatrix::beh(void)
{
    setMode(MOUTH_BEH);
}

void LedMatrix::smile(void)
{
    setMode(MOUTH_SMILE);
}

void LedMatrix::cylon(void)
{
    setMode(MOUTH_CYLON);
}

void LedMatrix::speak(void)
{
    setMode(MOUTH_SPEAK);
}

void LedMatrix::displayText(const char *text, bool scroll, unsigned int speed)
{
    _text = text;
    _scroll = scroll;
    _speed = speed;
    setMode(MOUTH_TEXT);
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

/*
 * LedMatrix.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef LEDMATRIX_HXX
#define LEDMATRIX_HXX

#include <memory>
#include <mutex>
#include <thread>

#define MAX7219_X_COUNT      4
#define MAX7219_Y_COUNT      4

using namespace std;

class LedMatrix {

public:

    LedMatrix();
    ~LedMatrix();

    void start(void);
    void stop(void);
    void join(void);

    unsigned int intensity(void) const;
    void setIntensity(unsigned int intensity);

    void clear(void);
    void setText(unsigned int y, const string &text,
                 unsigned int ttl = 30);
    void setWelcomeText(void);
    void setWelcomeText(unsigned int y, const string &text,
                        bool apply = false);
    unsigned int ttl(unsigned int y) const;
    void setDelay(unsigned int ms);
    unsigned int delay(void) const;
    void setSlowdownFactor(unsigned int y, unsigned int sf);
    unsigned int slowdownFactor(unsigned int y) const;

    void draw(unsigned int y, unsigned int x, const uint8_t fb[8]);
    void drawSL(unsigned int y, unsigned int x,
                unsigned int shift, bool clear,
                const uint8_t fb[8]);
    void drawSR(unsigned int y, unsigned int x,
                unsigned int shift, bool clear,
                const uint8_t fb[8]);
    void repaint(void);

private:

    static void *thread_func(void *);
    void run(void);

    int writeMax7219(const void *data, size_t size);
    int writeMax7219(uint8_t reg, uint8_t data);

    int _handle;
    int _fd;
    unsigned int _intensity;
    uint8_t _fb[4][4][8];

    bool _running;
    shared_ptr<thread> _thread;
    mutex _mutex;

    string _text[MAX7219_Y_COUNT];
    unsigned int _ttl[MAX7219_Y_COUNT];
    int _pos[MAX7219_Y_COUNT];
    int _slice[MAX7219_Y_COUNT];
    unsigned int _counter[MAX7219_Y_COUNT];
    unsigned int _reload[MAX7219_Y_COUNT];
    unsigned int _delay;

    string _welcome[MAX7219_Y_COUNT];

};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

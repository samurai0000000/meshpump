/*
 * LedMatrix.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef LEDMATRIX_HXX
#define LEDMATRIX_HXX

using namespace std;

class LedMatrix {

public:

    LedMatrix();
    ~LedMatrix();

    void stop(void);
    void join(void);

    unsigned int intensity(void) const;
    void setIntensity(unsigned int intensity);

    void draw(const uint32_t fb[8]);

    unsigned int mode(void) const;
    void setMode(unsigned int mode);
    void beh(void);
    void smile(void);
    void cylon(void);
    void speak(void);
    void displayText(const char *text,
                     bool scroll = true,
                     unsigned int speed = 1);

private:

    static void *thread_func(void *);
    void run(void);

    int writeMax7219(const void *data, size_t size);
    int writeMax7219(uint8_t reg, uint8_t data) const;

    void behCycle(unsigned int cycle);
    void smileCycle(unsigned int cycle);
    void cylonCycle(unsigned int cycle);
    void speakCycle(unsigned int cycle);
    void textCycle(unsigned int cycle);

#if defined(USE_PIGPIO)
    int _handle;
#else
    int _fd;
#endif
    unsigned int _intensity;
    uint32_t _fb[8];

    int _mode;
    std::string _text;
    bool _scroll;
    unsigned int _speed;

    bool _running;
    pthread_t _thread;
    pthread_mutex_t _mutex;
    pthread_cond_t _cond;

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

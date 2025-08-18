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

    void setText(unsigned int layer, const string &text);

    void draw(unsigned int layer, const uint32_t fb[8]);
    void draw(unsigned int i, unsigned int j, const uint8_t fb[8]);
    void repaint(void);

private:

    static void *thread_func(void *);
    void run(void);

    int writeMax7219(const void *data, size_t size);
    int writeMax7219(uint8_t reg, uint8_t data);

    int _handle;
    int _fd;
    unsigned int _intensity;
    uint32_t _fb[4][8];

    bool _running;
    pthread_t _thread;
    pthread_mutex_t _mutex;
    pthread_cond_t _cond;


    string _text[4];
    int _pos[4];

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

/*
 * meshpump.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <signal.h>
#include <getopt.h>
#include <iostream>
#include <vector>
#if defined(USE_PIGPIO)
#include <pigpiod_if.h>
#endif
#include <MeshPump.hxx>
#include <LedMatrix.hxx>

#define DEFAULT_DEVICE "/dev/ttyAMA0"

shared_ptr<MeshPump> pump = NULL;
shared_ptr<LedMatrix> leds = NULL;

static const struct option long_options[] = {
    { "device", required_argument, NULL, 'd', },
    { "verbose", no_argument, NULL, 'v', },
    { "log", no_argument, NULL, 'l', },
};

void sighandler(int signum)
{
    if (signum == SIGINT) {
        if (pump) {
            pump->detach();
        }
        if (leds) {
            leds->stop();
        }
    }
}

void cleanup(void)
{
#if defined(USE_PIGPIO)
    pigpio_stop();
#endif
}

int main(int argc, char **argv)
{
    int ret = 0;
    string device = DEFAULT_DEVICE;
    bool verbose = false;
    bool log = false;

    for (;;) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "d:vl",
                            long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'd':
            device = optarg;
            break;
        case 'v':
            verbose = true;
            break;
        case 'l':
            log = true;
            break;
        default:
            fprintf(stderr, "Unrecognized argument specified!\n");
            exit(EXIT_FAILURE);
            break;
        }
    }

#if defined(USE_PIGPIO)
    ret = pigpio_start(NULL, NULL);
    if (ret != 0) {
        cerr << "pgpiod_start failed!" << endl;
        ret = -1;
        exit(EXIT_FAILURE);
    }
#endif

    atexit(cleanup);

    pump = make_shared<MeshPump>();
    if (pump->attachSerial(device) == false) {
        cerr << "Unable to attch to " << device << endl;
        exit(EXIT_FAILURE);
    }

    pump->setVerbose(verbose);
    pump->enableLogStderr(log);

    leds = make_shared<LedMatrix>();

    signal(SIGINT, sighandler);

    if (pump) {
        pump->join();
    }

    if (leds) {
        leds->join();
    }

    cout << "Good-bye!" << endl;

    return ret;
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

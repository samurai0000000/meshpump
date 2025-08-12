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
#include <MeshShell.hxx>
#include "MeshPump.hxx"
#include "LedMatrix.hxx"

#define DEFAULT_DEVICE "/dev/ttyAMA0"

shared_ptr<MeshPump> pump = NULL;
shared_ptr<MeshShell> stdioShell = NULL;
shared_ptr<MeshShell> netShell = NULL;
shared_ptr<LedMatrix> leds = NULL;

static const struct option long_options[] = {
    { "device", required_argument, NULL, 'd', },
    { "stdio", no_argument, NULL, 's', },
    { "port", required_argument, NULL, 'p', },
    { "verbose", no_argument, NULL, 'v', },
    { "log", no_argument, NULL, 'l', },
};

void sighandler(int signum)
{
    (void)(signum);

    if (pump) {
        pump->detach();
    }
    if (stdioShell) {
        stdioShell->detach();
    }
    if (netShell) {
        netShell->detach();
    }
    if (leds) {
        leds->stop();
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
    uint16_t port = 0;
    bool verbose = false;
    bool log = false;

    for (;;) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "d:sp:vl",
                            long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'd':
            device = optarg;
            break;
        case 's':
            if (stdioShell == NULL) {
                stdioShell = make_shared<MeshShell>();
            }
            break;
        case 'p':
            port = atoi(optarg);
            if (netShell == NULL) {
                netShell = make_shared<MeshShell>();
            }
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
    signal(SIGINT, sighandler);

    pump = make_shared<MeshPump>();
    if (pump->attachSerial(device) == false) {
        cerr << "Unable to attch to " << device << endl;
        exit(EXIT_FAILURE);
    }

    pump->setClient(pump);
    pump->setVerbose(verbose);
    pump->enableLogStderr(log);

    if (netShell) {
        netShell->setClient(pump);
        netShell->setNVM(pump);
        netShell->bindPort(port);
    }

    if (stdioShell) {
        stdioShell->setClient(pump);
        stdioShell->setNVM(pump);
        stdioShell->attachStdio();
    }

    leds = make_shared<LedMatrix>();

    /* ------- */

    if (pump) {
        pump->join();
    }
    if (stdioShell) {
        stdioShell->join();
    }
    if (netShell) {
        netShell->join();
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

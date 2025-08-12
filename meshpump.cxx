/*
 * meshpump.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <libconfig.h++>
#if defined(USE_PIGPIO)
#include <pigpiod_if.h>
#endif
#include <iostream>
#include <vector>
#include <algorithm>
#include <MeshShell.hxx>
#include "MeshPump.hxx"
#include "LedMatrix.hxx"
#include "version.h"

using namespace libconfig;

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

static void loadLibConfig(Config &cfg, string &path)
{
    int fd;

    if (path.empty()) {
        string home;

        home = getenv("HOME");
        if (home.empty()) {
            return;
        }

        path = home + "/.meshpump";
    }

    // 'touch' to test the path validity
    fd = open(path.c_str(),
              O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK,
              0666);
    if (fd == -1) {
        goto done;
    } else {
        close(fd);
        fd = -1;
    }

    try {
        cfg.readFile(path.c_str());
    } catch (FileIOException &e) {
    } catch (ParseException &e) {
    }

done:

    return;
}

int main(int argc, char **argv)
{
    int ret = 0;
    Config cfg;
    string cfgfile;
    string device = DEFAULT_DEVICE;
    bool useStdioShell = false;
    uint16_t port = 0;
    bool verbose = false;
    bool log = false;
    string banner;
    string version;
    string built;
    string copyright;

    banner = "The MeshPump Application";
    version = string("Version: ") + string(MYPROJECT_VERSION_STRING);
    built = string("Built: ") + string(MYPROJECT_WHOAMI) + string("@") +
        string(MYPROJECT_HOSTNAME) + string(" ") + string(MYPROJECT_DATE);
    copyright = string("Copyright (C) 2025, Charles Chiou");

    loadLibConfig(cfg, cfgfile);

    try {
        Setting &root = cfg.getRoot();
        root.lookupValue("device", device);
    } catch (SettingNotFoundException &e) {
    } catch (SettingTypeException &e) {
    }

    try {
        int cfgStdioShell = 0;
        Setting &root = cfg.getRoot();
        root.lookupValue("stdioShell", cfgStdioShell);
        useStdioShell = cfgStdioShell != 0 ? true : false;
    } catch (SettingNotFoundException &e) {
    } catch (SettingTypeException &e) {
    }

    try {
        int cfgDeviceLog = 0;
        Setting &root = cfg.getRoot();
        root.lookupValue("deviceLog", cfgDeviceLog);
        log = cfgDeviceLog != 0 ? true : false;
    } catch (SettingNotFoundException &e) {
    } catch (SettingTypeException &e) {
    }

    try {
        int cfgPort = 0;
        Setting &root = cfg.getRoot();
        root.lookupValue("port", cfgPort);
        port = cfgPort;
    } catch (SettingNotFoundException &e) {
    } catch (SettingTypeException &e) {
    }

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
            useStdioShell = true;
            break;
        case 'p':
            port = atoi(optarg);
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

    if (device.empty()) {
        device = DEFAULT_DEVICE;
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

    if (port != 0) {
        netShell = make_shared<MeshShell>();
        netShell->setBanner(banner);
        netShell->setVersion(version);
        netShell->setBuilt(built);
        netShell->setCopyright(copyright);
        netShell->setClient(pump);
        netShell->setNVM(pump);
        netShell->bindPort(port);
    }


    if (useStdioShell) {
        stdioShell = make_shared<MeshShell>();
        stdioShell->setBanner(banner);
        stdioShell->setVersion(version);
        stdioShell->setBuilt(built);
        stdioShell->setCopyright(copyright);
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

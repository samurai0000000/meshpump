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
#include "MeshPump.hxx"
#include "LedMatrix.hxx"
#include <MeshPumpShell.hxx>
#include "version.h"

using namespace libconfig;

#define DEFAULT_DEVICE "/dev/ttyAMA0"

shared_ptr<MeshPump> meshpump = NULL;
shared_ptr<LedMatrix> ledMatrix = NULL;
static shared_ptr<MeshPumpShell> stdioShell = NULL;
static shared_ptr<MeshPumpShell> netShell = NULL;

void sighandler(int signum)
{
    (void)(signum);

    if (meshpump) {
        meshpump->detach();
    }
    if (stdioShell) {
        stdioShell->detach();
    }
    if (netShell) {
        netShell->detach();
    }
    if (ledMatrix) {
        ledMatrix->stop();
    }
}

void cleanup(void)
{
    meshpump->setFishPumpOnOff(true);
    meshpump->setUpPumpOnOff(false);
    meshpump->setLightingOnOff(false);

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

static const struct option long_options[] = {
    { "device", required_argument, NULL, 'd', },
    { "stdio", no_argument, NULL, 's', },
    { "port", required_argument, NULL, 'p', },
    { "daemon", no_argument, NULL, 'b', },
    { "verbose", no_argument, NULL, 'v', },
    { "log", no_argument, NULL, 'l', },
};

int main(int argc, char **argv)
{
    int ret = 0;
    Config cfg;
    string cfgfile;
    string device = DEFAULT_DEVICE;
    bool useStdioShell = false;
    uint16_t port = 0;
    bool daemon = false;
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

    try {
        bool cfgDaemon = 0;
        Setting &root = cfg.getRoot();
        root.lookupValue("daemon", cfgDaemon);
        daemon = cfgDaemon;
    } catch (SettingNotFoundException &e) {
    } catch (SettingTypeException &e) {
    }

    for (;;) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "d:sp:bvl",
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
        case 'b':
            daemon = true;
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

    if (daemon) {
        pid_t pid;
        int fdevnull;

        useStdioShell = false;
        verbose = false;
        if (port == 0) {
            port = 16876;
        }

        pid = fork();
        if (pid == -1) {
            cerr << "fork failed!" << endl;
            exit(EXIT_FAILURE);
        } else if (pid  != 0) {
            exit(EXIT_SUCCESS);
        } else {
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            fdevnull = open("/dev/null", O_WRONLY);
            if (fdevnull != -1) {
                dup2(fdevnull, STDOUT_FILENO);
                dup2(fdevnull, STDERR_FILENO);
                close(fdevnull);
            }
        }
    }

    atexit(cleanup);
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGPIPE, SIG_IGN);

    ledMatrix = make_shared<LedMatrix>();
    ledMatrix->setText(0, copyright);
    ledMatrix->setText(1, built);
    ledMatrix->setText(2, version);
    ledMatrix->setText(3, banner);
    ledMatrix->start();

    meshpump = make_shared<MeshPump>();
    meshpump->setBanner(banner);
    meshpump->setVersion(version);
    meshpump->setBuilt(built);
    meshpump->setCopyright(copyright);
    if (meshpump->attachSerial(device) == false) {
        cerr << "Unable to attch to " << device << endl;
        exit(EXIT_FAILURE);
    }

    meshpump->setClient(meshpump);
    meshpump->setNvm(meshpump);
    meshpump->setVerbose(verbose);
    meshpump->enableLogStderr(log);

    if (port != 0) {
        netShell = make_shared<MeshPumpShell>();
        netShell->setClient(meshpump);
        netShell->setNvm(meshpump);
        netShell->bindPort(port);
    }


    if (useStdioShell) {
        stdioShell = make_shared<MeshPumpShell>();
        stdioShell->setClient(meshpump);
        stdioShell->setNvm(meshpump);
        stdioShell->attachStdio();
    }

    /* ------- */

    if (meshpump) {
        meshpump->join();
    }
    if (stdioShell) {
        stdioShell->join();
    }
    if (netShell) {
        netShell->join();
    }
    if (ledMatrix) {
        ledMatrix->join();
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

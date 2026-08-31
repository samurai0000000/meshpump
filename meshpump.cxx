/*
 * meshpump.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libconfig.h++>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
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
static volatile sig_atomic_t g_stop = 0;
static int g_stop_pipe[2] = { -1, -1 };

void sighandler(int signum)
{
    char c = 1;

    (void)(signum);
    g_stop = 1;
    if (g_stop_pipe[1] != -1) {
        (void) write(g_stop_pipe[1], &c, 1);
    }
}

static void requestStop(void)
{
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

static void stopWatcher(void)
{
    char c;

    while (!g_stop) {
        ssize_t n = read(g_stop_pipe[0], &c, 1);
        if (n > 0) {
            break;
        }
        if ((n < 0) && (errno == EINTR)) {
            continue;
        }
        break;
    }
    requestStop();
}

static bool parsePort(const char *s, uint16_t &port)
{
    char *end = NULL;
    unsigned long v;

    if ((s == NULL) || (s[0] == '\0')) {
        return false;
    }

    errno = 0;
    v = strtoul(s, &end, 10);
    if ((errno != 0) || (end == s) || (*end != '\0') || (v > 65535)) {
        return false;
    }

    port = (uint16_t) v;
    return true;
}

static void releaseMeshPump(void)
{
    stdioShell.reset();
    netShell.reset();
    if (meshpump) {
        meshpump->setClient(NULL);
        meshpump->setNvm(NULL);
    }
    meshpump.reset();
    ledMatrix.reset();
}

void cleanup(void)
{
    if (meshpump) {
        meshpump->setFishPumpOnOff(true);
        meshpump->setUpPumpOnOff(false);
        meshpump->setLightingOnOff(false);
    }
}

static void loadLibConfig(Config &cfg, string &path)
{
    int fd;

    if (path.empty()) {
        const char *homedir;

        homedir = getenv("HOME");
        if ((homedir == NULL) || (homedir[0] == '\0')) {
            return;
        }

        path = string(homedir) + "/.meshpump";
    }

    // 'touch' to test the path validity
    fd = open(path.c_str(),
              O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK,
              S_IRUSR | S_IWUSR);
    if (fd == -1) {
        cerr << path << ": " << strerror(errno) << endl;
        return;
    }
    if (fchmod(fd, S_IRUSR | S_IWUSR) == -1) {
        cerr << path << ": " << strerror(errno) << endl;
        close(fd);
        return;
    }
    close(fd);

    {
        struct stat st;

        if ((stat(path.c_str(), &st) == 0) && (st.st_size == 0)) {
            return;
        }
    }

    try {
        cfg.readFile(path.c_str());
    } catch (const FileIOException &) {
        cerr << "Unable to read config " << path << endl;
        exit(EXIT_FAILURE);
    } catch (ParseException &e) {
        cerr << "Parse error in " << e.getFile()
             << " line " << e.getLine() << ": " << e.getError() << endl;
        exit(EXIT_FAILURE);
    }
}

static const struct option long_options[] = {
    { "device", required_argument, NULL, 'd', },
    { "stdio", no_argument, NULL, 's', },
    { "port", required_argument, NULL, 'p', },
    { "daemon", no_argument, NULL, 'b', },
    { "log", no_argument, NULL, 'l', },
    { 0, 0, 0, 0 },
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
        if (root.lookupValue("port", cfgPort)) {
            if ((cfgPort < 0) || (cfgPort > 65535)) {
                cerr << "Invalid config port: " << cfgPort << endl;
                exit(EXIT_FAILURE);
            }
            port = (uint16_t) cfgPort;
        }
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
        int c = getopt_long(argc, argv, "d:sp:bl",
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
            if (!parsePort(optarg, port)) {
                cerr << "Invalid port: " << optarg << endl;
                exit(EXIT_FAILURE);
            }
            break;
        case 'b':
            daemon = true;
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

    if (daemon) {
        pid_t pid;
        int fdevnull;

        useStdioShell = false;
        if (port == 0) {
            port = 16876;
        }

        pid = fork();
        if (pid == -1) {
            cerr << "fork failed!" << endl;
            exit(EXIT_FAILURE);
        } else if (pid != 0) {
            exit(EXIT_SUCCESS);
        }

        if (setsid() == -1) {
            exit(EXIT_FAILURE);
        }

        fdevnull = open("/dev/null", O_RDWR);
        if (fdevnull != -1) {
            dup2(fdevnull, STDIN_FILENO);
            dup2(fdevnull, STDOUT_FILENO);
            dup2(fdevnull, STDERR_FILENO);
            if (fdevnull > STDERR_FILENO) {
                close(fdevnull);
            }
        }
    }

    atexit(cleanup);

    if (pipe(g_stop_pipe) == -1) {
        cerr << "pipe failed: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    fcntl(g_stop_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(g_stop_pipe[1], F_SETFD, FD_CLOEXEC);
    {
        int flags = fcntl(g_stop_pipe[1], F_GETFL, 0);
        if (flags != -1) {
            fcntl(g_stop_pipe[1], F_SETFL, flags | O_NONBLOCK);
        }
    }

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGPIPE, SIG_IGN);

    thread stopThread(stopWatcher);

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
        cerr << "Unable to attach to " << device << endl;
        ret = EXIT_FAILURE;
        requestStop();
    } else if (g_stop) {
        meshpump->detach();
        ret = EXIT_FAILURE;
    } else {
        meshpump->setClient(meshpump);
        meshpump->setNvm(meshpump);
        meshpump->enableLogStderr(log);

        if (port != 0) {
            netShell = make_shared<MeshPumpShell>();
            netShell->setClient(meshpump);
            netShell->setNvm(meshpump);
            if (!netShell->bindPort(port)) {
                netShell.reset();
            }
        }

        if (useStdioShell && !g_stop) {
            stdioShell = make_shared<MeshPumpShell>();
            stdioShell->setClient(meshpump);
            stdioShell->setNvm(meshpump);
            stdioShell->attachStdio();
        }
    }

    if (g_stop) {
        requestStop();
    }

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

    if (ret == 0) {
        cout << "Good-bye!" << endl;
    }

    g_stop = 1;
    if (g_stop_pipe[1] != -1) {
        char c = 1;
        (void) write(g_stop_pipe[1], &c, 1);
    }
    if (stopThread.joinable()) {
        stopThread.join();
    }
    if (g_stop_pipe[0] != -1) {
        close(g_stop_pipe[0]);
        g_stop_pipe[0] = -1;
    }
    if (g_stop_pipe[1] != -1) {
        close(g_stop_pipe[1]);
        g_stop_pipe[1] = -1;
    }

    releaseMeshPump();

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

/*
 * MeshPumpShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <string>
#include <stdexcept>
#include <cctype>
#include <MeshPump.hxx>
#include <LedMatrix.hxx>
#include <MeshPumpShell.hxx>

extern shared_ptr<MeshPump> meshpump;
extern shared_ptr<LedMatrix> ledMatrix;

MeshPumpShell::MeshPumpShell(shared_ptr<MeshClient> client)
    : MeshShell(client)
{
    _help_list.push_back("led");
    _help_list.push_back("pump");
    _help_list.push_back("lighting");
}

MeshPumpShell::~MeshPumpShell()
{

}

shared_ptr<MeshShell> MeshPumpShell::newInstance(void)
{
    return make_shared<MeshPumpShell>(_client);
}

int MeshPumpShell::system(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        return MeshShell::system(argc, argv);
    }

    shared_ptr<MeshPump> mp = dynamic_pointer_cast<MeshPump>(_client);
    if (!mp) {
        mp = meshpump;
    }
    static const char *pump_argv[] = { "pump", };
    static const char *lighting_argv[] = { "lighting", };

    MeshShell::system(argc, argv);
    if (mp == NULL) {
        return 0;
    }

    this->pump(1, (char **) pump_argv);
    this->lighting(1, (char **) lighting_argv);
    this->printf("CPU temp: %.1fC\n", mp->getCpuTempC());

    return 0;
}

static bool parseStrictInt(const char *s, int &val)
{
    string str;
    size_t idx = 0;

    if (s == NULL || s[0] == '\0') {
        return false;
    }
    str = s;
    for (size_t i = 0; i < str.size(); i++) {
        if (i == 0 && (str[i] == '-' || str[i] == '+')) {
            if (str.size() == 1) {
                return false;
            }
            continue;
        }
        if (!isdigit((unsigned char) str[i])) {
            return false;
        }
    }

    try {
        val = stoi(str, &idx);
        if (idx != str.size()) {
            return false;
        }
    } catch (const invalid_argument &) {
        return false;
    } catch (const out_of_range &) {
        return false;
    }

    return true;
}

static int getArgY(const char *s)
{
    int ret = -1;

    if (!parseStrictInt(s, ret)) {
        return -1;
    }
    if ((ret < 0) || (ret >= MAX7219_Y_COUNT)) {
        return -1;
    }

    return ret;
}

int MeshPumpShell::led(int argc, char **argv)
{
    int ret = 0;
    string message;
    int startArg = 1;
    int y = 0;

    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [command|row] [args...]\n", argv[0]);
        this->printf("  Configure or display text on the LED matrix.\n");
        this->printf("Commands:\n");
        this->printf("  led                          Show delay, row TTL, and slowdown factors\n");
        this->printf("  led delay <ms>               Set scroll step delay in milliseconds\n");
        this->printf("  led sf <row> <sf>            Set slowdown factor for specified row\n");
        this->printf("  led blank                    Clear LED display\n");
        this->printf("  led welcome                  Display welcome text\n");
        this->printf("  led <row> <text>             Display text on specified row\n");
        return 0;
    }

    if (!ledMatrix) {
        this->printf("led matrix not initialized!\n");
        return -1;
    }

    if (argc == 1) {
        this->printf("delay: %ums\n", ledMatrix->delay());
        for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
            this->printf("row %u: ", y);
            this->printf("ttl=%us, ", ledMatrix->ttl(y));
            this->printf("sf=%u\n", ledMatrix->slowdownFactor(y));
        }
        goto done;
    } else if ((argc == 3) && (strcmp(argv[1], "delay") == 0)) {
        int ms = 0;
        if (!parseStrictInt(argv[2], ms) || (ms <= 0)) {
            ret = -1;
            this->printf("delay ms=%s is invalid!\n", argv[2]);
            goto done;
        }

        ledMatrix->setDelay((unsigned int) ms);
        this->printf("set delay to %ums\n", ms);
        goto done;
    } else if ((argc == 4) && (strcmp(argv[1], "sf") == 0) &&
               ((y = getArgY(argv[2])) != -1)) {
        int sf = 0;
        if (!parseStrictInt(argv[3], sf) || (sf < 1)) {
            ret = -1;
            this->printf("sf=%s is invalid!\n", argv[3]);
            goto done;
        }

        ledMatrix->setSlowdownFactor(y, (unsigned int) sf);
        this->printf("set sf of row %u to %u\n", y, sf);
        goto done;
    } else if ((argc == 2) && (strcmp(argv[1], "blank") == 0)) {
        ledMatrix->clear();
        goto done;
    } else if ((argc == 2) && (strcmp(argv[1], "welcome") == 0)) {
        ledMatrix->setWelcomeText();
        goto done;
    } else if ((argc > 2) && ((y = getArgY(argv[1])) != -1)) {
        startArg = 2;
    } else {
        goto done;
    }

    for (int i = startArg; i < argc; i++) {
        if (i > startArg) {
            message += " ";
        }
        message += argv[i];
    }

    if (message == "\"\"") {
        message = "";  // This is a temporary hack
    }

    if (ledMatrix) {
        ledMatrix->setText((unsigned int) y, message);
    }

done:

    return ret;
}

int MeshPumpShell::pump(int argc, char **argv)
{
    int ret = 0;
    bool isFish = false;
    bool isUp = false;
    bool onOff = false;
    unsigned int cutoff = 0;

    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [fish|up] [on|off] [cutoff_sec]\n", argv[0]);
        this->printf("  Control water pumps or show pump status.\n");
        this->printf("Commands:\n");
        this->printf("  pump                         Show pump status and auto-cutoff settings\n");
        this->printf("  pump fish on|off             Turn fish pump on or off\n");
        this->printf("  pump up on|off [cutoff_sec]  Turn up pump on or off (with optional cutoff duration in sec)\n");
        return 0;
    }

    shared_ptr<MeshPump> mp = dynamic_pointer_cast<MeshPump>(_client);
    if (!mp) {
        mp = meshpump;
    }
    if (!mp) {
        this->printf("meshpump not initialized!\n");
        return -1;
    }

    if (argc == 1) {
        this->printf("fish-pump: %s\n",
                     mp->isFishPumpOn() ? "on" : "off");
        this->printf("up-pump: %s\n",
                     mp->isUpPumpOn() ? "on" : "off");
        this->printf("up-pump auto cutoff: %u\n",
                     mp->getUpPumpAutoCutoffSec());
    } else {
        if ((argc > 1) &&
            ((strcasecmp(argv[1], "0") == 0) ||
             (strcasecmp(argv[1], "fish") == 0) ||
             (strcasecmp(argv[1], "fish-pump") == 0))) {
            isFish = true;
        } else if ((argc > 1) &&
                   ((strcasecmp(argv[1], "1") == 0) ||
                    (strcasecmp(argv[1], "up") == 0) ||
                    (strcasecmp(argv[1], "up-pump") == 0))) {
            isUp = true;
        } else {
            ret = -1;
            this->printf("no pump specified!\n");
            goto done;
        }

        if ((argc > 2) && (strcasecmp(argv[2], "on") == 0)) {
            onOff = true;
        } else if ((argc > 2) && (strcasecmp(argv[2], "off") == 0)) {
            onOff = false;
        } else {
            ret = -1;
            this->printf("no on/off specified!\n");
            goto done;
        }

        if (isUp && (onOff == true) && (argc > 3)) {
            int parsedCutoff = 0;
            bool allDigits = (argv[3][0] != '\0');

            for (size_t i = 0; argv[3][i] != '\0'; i++) {
                if (!isdigit((unsigned char) argv[3][i])) {
                    allDigits = false;
                    break;
                }
            }

            if (!allDigits || !parseStrictInt(argv[3], parsedCutoff) ||
                (parsedCutoff < MIN_UPPUMP_AUTO_CUTOFF_SEC)) {
                ret = -1;
                this->printf("cutoff '%s' argument is invalid!\n", argv[3]);
                goto done;
            }

            cutoff = (unsigned int) parsedCutoff;
            if (cutoff > MAX_UPPUMP_AUTO_CUTOFF_SEC) {
                ret = -1;
                this->printf("cut-off of %u seconds is too big!\n", cutoff);
                goto done;
            }
        }

        if (isFish) {
            mp->setFishPumpOnOff(onOff);
            this->printf("set fish-pump to %s\n", onOff ? "on" : "off");
        } else if (isUp) {
            if (onOff == false) {
                mp->setUpPumpOnOff(false);
                this->printf("set up-pump to off\n");
            } else {
                if (cutoff == 0) {
                    mp->setUpPumpOnOff(true);
                    this->printf("set up-pump to on for %u seconds\n",
                                 mp->getUpPumpAutoCutoffSec());
                } else {
                    mp->setUpPumpOnWithCutoffSec(cutoff);
                    this->printf("set up-pump to on for %u seconds\n",
                                 cutoff);
                }
            }
        }
    }

done:

    return ret;
}

int MeshPumpShell::lighting(int argc, char **argv)
{
    int ret = 0;

    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [on|off]\n", argv[0]);
        this->printf("  Control lighting or show lighting status.\n");
        this->printf("Commands:\n");
        this->printf("  lighting                     Show lighting status\n");
        this->printf("  lighting on|off              Turn lighting on or off\n");
        return 0;
    }

    shared_ptr<MeshPump> mp = dynamic_pointer_cast<MeshPump>(_client);
    if (!mp) {
        mp = meshpump;
    }
    if (!mp) {
        this->printf("meshpump not initialized!\n");
        return -1;
    }

    if (argc == 1) {
        this->printf("lighting: %s\n", mp->isLightingOn() ?
                     "on" : "off");
    } else if ((argc == 2) && (strcasecmp(argv[1], "on") == 0)) {
        mp->setLightingOnOff(true);
    } else if ((argc == 2) && (strcasecmp(argv[1], "off") == 0)) {
        mp->setLightingOnOff(false);
    } else {
        this->printf("syntax error!\n");
        ret = -1;
        goto done;
    }

done:

    return ret;
}

int MeshPumpShell::unknown_command(int argc, char **argv)
{
    int ret = 0;

    if (strcmp(argv[0], "led") == 0) {
        ret = this->led(argc, argv);
    } else if (strcmp(argv[0], "pump") == 0) {
        ret = this->pump(argc, argv);
    } else if (strcmp(argv[0], "lighting") == 0) {
        ret = this->lighting(argc, argv);
    } else {
        ret = MeshShell::unknown_command(argc, argv);
    }

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

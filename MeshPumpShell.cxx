/*
 * MeshPumpShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <string>
#include <MeshPump.hxx>
#include <LedMatrix.hxx>
#include <MeshPumpShell.hxx>

extern shared_ptr<MeshPump> meshpump;
extern shared_ptr<LedMatrix> ledMatrix;

MeshPumpShell::MeshPumpShell(shared_ptr<MeshClient> client)
    : MeshShell(client)
{
    _help_list.push_back("led");
}

MeshPumpShell::~MeshPumpShell()
{

}

int MeshPumpShell::system(int argc, char **argv)
{
    shared_ptr<MeshPump> meshpump = dynamic_pointer_cast<MeshPump>(_client);
    static const char *pump_argv[] = { "pump", };

    MeshShell::system(argc, argv);
    pump(1, (char **) pump_argv);
    this->printf("CPU temp: %.1fC\n", meshpump->getCpuTempC());

    return 0;
}

static int getArgY(const char *s)
{
    int ret = 0;

    try {
        ret = stoi(s);
        if ((ret < 0) || (ret >= MAX7219_Y_COUNT)) {
            ret = -1;
        }
    } catch (const invalid_argument &e) {
        ret = -1;
    }

    return ret;
}

int MeshPumpShell::led(int argc, char **argv)
{
    int ret = 0;
    string message;
    int startArg = 1;
    int y = 0;

    if (argc == 1) {
        this->printf("delay: %ums\n", ledMatrix->delay());
        for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
            this->printf("row %u: ", y);
            this->printf("ttl=%us, ", ledMatrix->ttl(y));
            this->printf("sf=%u\n", ledMatrix->slowdownFactor(y));
        }
        goto done;
    } else if ((argc == 3) && (strcmp(argv[1], "delay") == 0)) {
        try {
            int ms = stoi(argv[2]);
            if (ms <= 0) {
                ret = -1;
                this->printf("delay ms=%s is invalid!\n", argv[2]);
                goto done;
            }

            ledMatrix->setDelay((unsigned int) ms);
            this->printf("set delay to %ums\n", ms);
            goto done;
        } catch (const invalid_argument &e) {
            ret = -1;
            this->printf("delay ms=%s is invalid!\n", argv[2]);
            goto done;
        }
    } else if ((argc == 4) && (strcmp(argv[1], "sf") == 0) &&
               ((y = getArgY(argv[2])) != -1)) {
        try {
            int sf = stoi(argv[3]);
            if (sf < 1) {
                ret = -1;
                this->printf("sf=%s is invalid!\n", argv[3]);
                goto done;
            }

            ledMatrix->setSlowdownFactor(y, (unsigned int) sf);
            this->printf("set sf of row %u to %u\n", y, sf);
            goto done;
        } catch (const invalid_argument &e) {
            ret = -1;
            this->printf("sf=%s is invalid!\n", argv[3]);
            goto done;
        }
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

    if (argc == 1) {
        this->printf("fish-pump: %s\n",
                     meshpump->isFishPumpOn() ? "on" : "off");
        this->printf("up-pump: %s\n",
                     meshpump->isUpPumpOn() ? "on" : "off");
        this->printf("up-pump auto cutoff: %u\n",
                     meshpump->getUpPumpAutoCutoffSec());
    } else {
        if ((argc > 1) &&
            ((strcasecmp(argv[1], "0") == 0) ||
             (strcasecmp(argv[1], "fish") == 0) ||
             (strcasecmp(argv[1], "fish-pump") == 0))) {
            isFish = true;
        } else if ((argc > 1) &&
                   ((strcasecmp(argv[1], "0") == 0) ||
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
            try {
                cutoff = stoi(argv[3]);
            } catch (const invalid_argument &e) {
                ret = -1;
                this->printf("cutoff '%s' argument is invalid!\n", argv[3]);
                goto done;
            }

            if (cutoff > 120) {
                ret = -1;
                this->printf("cut-off of %u seconds is too big!\n", cutoff);
                goto done;
            }
        }

        if (isFish) {
            meshpump->setFishPumpOnOff(onOff);
            this->printf("set fish-pump to %s\n", onOff ? "on" : "off");
        } else if (isUp) {
            if (onOff == false) {
                meshpump->setUpPumpOnOff(false);
                this->printf("set up-pump to off\n");
            } else {
                if (cutoff == 0) {
                    meshpump->setUpPumpOnOff(true);
                    this->printf("set up-pump to on for %u seconds\n",
                                 meshpump->getUpPumpAutoCutoffSec());
                } else {
                    meshpump->setUpPumpOnWithCutoffSec(cutoff);
                    this->printf("set up-pump to on for %u seconds\n",
                                 cutoff);
                }
            }
        }
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

/*
 * MeshPump.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <lgpio.h>
#include <csignal>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <MeshPump.hxx>
#include <LedMatrix.hxx>
#include "version.h"

extern shared_ptr<MeshPump> meshpump;
extern shared_ptr<LedMatrix> ledMatrix;

MeshPump::MeshPump()
    : MeshClient()
{
    int status;

    _gpiochip = -1;
    signal(SIGALRM, alarmHandler);

    _gpiochip = openGpiochip();
    if (_gpiochip < 0) {
        cerr << "lgGpiochipOpen failed: " << lguErrorText(_gpiochip) << endl;
        exit(EXIT_FAILURE);
    }

    status = lgGpioClaimOutput(_gpiochip, 0, RELAY1_PIN, 0);
    if (status < 0) {
        cerr << "lgGpioClaimOutput " << RELAY1_PIN << " failed: "
             << lguErrorText(status) << endl;
        exit(EXIT_FAILURE);
    }
    status = lgGpioClaimOutput(_gpiochip, 0, RELAY2_PIN, 1);
    if (status < 0) {
        cerr << "lgGpioClaimOutput " << RELAY2_PIN << " failed: "
             << lguErrorText(status) << endl;
        exit(EXIT_FAILURE);
    }
    status = lgGpioClaimOutput(_gpiochip, 0, RELAY3_PIN, 1);
    if (status < 0) {
        cerr << "lgGpioClaimOutput " << RELAY3_PIN << " failed: "
             << lguErrorText(status) << endl;
        exit(EXIT_FAILURE);
    }

    setFishPumpOnOff(true);
    setUpPumpOnOff(false);
    setUpPumpAutoCutoffSec(10);
    setLightingOnOff(false);
}

MeshPump::~MeshPump()
{
    setFishPumpOnOff(true);
    setUpPumpOnOff(false);
    setLightingOnOff(false);

    if (_gpiochip >= 0) {
        lgGpioFree(_gpiochip, RELAY1_PIN);
        lgGpioFree(_gpiochip, RELAY2_PIN);
        lgGpioFree(_gpiochip, RELAY3_PIN);
        lgGpiochipClose(_gpiochip);
        _gpiochip = -1;
    }
}

int MeshPump::openGpiochip(void)
{
    lgChipInfo_t info;
    int h, i, status;

    for (i = 0; i < 8; i++) {
        h = lgGpiochipOpen(i);
        if (h < 0) {
            continue;
        }

        status = lgGpioGetChipInfo(h, &info);
        if ((status == LG_OKAY) &&
            (strstr(info.label, "pinctrl") != NULL)) {
            return h;
        }

        lgGpiochipClose(h);
    }

    return lgGpiochipOpen(0);
}

void MeshPump::gpioWrite(int pin, int level)
{
    if (_gpiochip >= 0) {
        lgGpioWrite(_gpiochip, pin, level);
    }
}

void MeshPump::join(void)
{
    MeshClient::join();
}

void MeshPump::setClient(shared_ptr<SimpleClient> client)
{
    if (client && (client.get() == static_cast<SimpleClient *>(this))) {
        // Non-owning: HomeChat must not keep a shared_ptr to *this
        HomeChat::setClient(shared_ptr<SimpleClient>(
                                shared_ptr<SimpleClient>(), this));
        return;
    }

    HomeChat::setClient(client);
}

void MeshPump::setNvm(shared_ptr<BaseNvm> nvm)
{
    if (nvm && (nvm.get() == static_cast<BaseNvm *>(this))) {
        // Non-owning: HomeChat and SimpleClient must not keep a shared_ptr to *this
        shared_ptr<BaseNvm> nonOwning(shared_ptr<BaseNvm>(), this);
        HomeChat::setNvm(nonOwning);
        SimpleClient::setNvm(nonOwning);
        return;
    }

    HomeChat::setNvm(nvm);
    SimpleClient::setNvm(nvm);
}

void MeshPump::gotConfigCompleteId(uint32_t id)
{
    if (setupFor(whoami()) == true) {
        if (loadNvm() == false) {
            saveNvm();
        }
        syncFromNvm();
    }

    MeshClient::gotConfigCompleteId(id);
}

void MeshPump::gotRebooted(bool rebooted)
{
    MeshClient::gotRebooted(rebooted);
}

void MeshPump::loop(void)
{

}

bool MeshPump::isFishPumpOn(void) const
{
    return _fishPump;
}

void MeshPump::setFishPumpOnOff(bool onOff)
{
    _fishPump = onOff;
    gpioWrite(RELAY1_PIN, !onOff);
    if (ledMatrix) {
        if (onOff) {
            ledMatrix->setText(3, "  ON", 60);
        } else {
            ledMatrix->setText(3, " OFF", UINT_MAX);
        }
    }
}

void MeshPump::alarmHandler(int signum)
{
    if (signum == SIGALRM) {
        meshpump->setUpPumpOnOff(false);
    }
}

bool MeshPump::isUpPumpOn(void) const
{
    return _upPump;
}

void MeshPump::setUpPumpOnOff(bool onOff)
{
    if (onOff) {
        setUpPumpOnWithCutoffSec(getUpPumpAutoCutoffSec());
    } else {
        gpioWrite(RELAY2_PIN, !onOff);
        if (ledMatrix) {
            ledMatrix->setText(2, " OFF", 60);
        }
    }
}

void MeshPump::setUpPumpOnWithCutoffSec(unsigned int seconds)
{
    if (seconds > MAX_UPPUMP_AUTO_CUTOFF_SEC) {
        goto done;
    }

    _upPump = true;
    gpioWrite(RELAY2_PIN, !_upPump);
    if (ledMatrix) {
        ledMatrix->setText(2, "  ON", UINT_MAX);
    }
    alarm(seconds);

done:

    return;
}

unsigned int MeshPump::getUpPumpAutoCutoffSec(void) const
{
    return _upPumpAutoCutoffSec;
}

void MeshPump::setUpPumpAutoCutoffSec(unsigned int seconds)
{
    if (seconds > MAX_UPPUMP_AUTO_CUTOFF_SEC) {
        goto done;
    }

    _upPumpAutoCutoffSec = seconds;

done:

    return;
}

bool MeshPump::isLightingOn(void) const
{
    return _lighting;
}

void MeshPump::setLightingOnOff(bool onOff)
{
    _lighting = onOff;
    gpioWrite(RELAY3_PIN, !onOff);
    if (onOff) {
        ledMatrix->setText(1, "  ON", 60);
    } else {
        ledMatrix->setText(1, " OFF", 60);
    }
}

void MeshPump::gotTextMessage(const meshtastic_MeshPacket &packet,
                             const string &message)
{
    bool result = false;

    MeshClient::gotTextMessage(packet, message);
    result = handleTextMessage(packet, message);
    if (result) {
        return;
    }
}

void MeshPump::crontab(const struct tm *now)
{
    int hour = now->tm_hour;
    bool shouldTurnOn = false;

    if ((hour <= 5) || (hour > 18)) {
        shouldTurnOn = true;
    } else {
        shouldTurnOn = false;
    }

    if (shouldTurnOn != isLightingOn()) {
        setLightingOnOff(shouldTurnOn);
    }
}

bool MeshPump::loadNvm(void)
{
    bool result;

    result = MeshNvm::loadNvm();

    return result;
}

bool MeshPump::saveNvm(void)
{
    bool result;

    result = MeshNvm::saveNvm();

    return result;
}

float MeshPump::getCpuTempC(void)
{
#define MAX_STRING        1024
#define GET_GENCMD_RESULT 0x00030080
    float tempC = 0.0;
    int fd = -1;
    int ret;
    static const char *command = "measure_temp";
    unsigned p[(MAX_STRING >> 2) + 7];
    unsigned int i = 0;
    const char *s;
    string str;

    fd = open("/dev/vcio", 0);
    if (fd == -1) {
        fprintf(stderr, "open: %s!\n", strerror(errno));
        goto done;
    }

    i = 0;
    p[i++] = 0; // size
    p[i++] = 0x00000000; // process request
    p[i++] = GET_GENCMD_RESULT; // (the tag id)
    p[i++] = MAX_STRING;// buffer_len
    p[i++] = 0; // request_len (set to response length)
    p[i++] = 0; // error repsonse
    memcpy(p + i, command, strlen(command) + 1);
    i += MAX_STRING >> 2;
    p[i++] = 0x00000000; // end tag
    p[0] = i * sizeof(*p); // actual size

    ret = ioctl(fd, _IOWR(100, 0, char *), p);
    if (ret == -1) {
        fprintf(stderr, "ioctl: %s!\n", strerror(errno));
        goto done;
    }

    s = (const char *) (p + 6);
    {
        size_t slen = sizeof(p) - ((const char *) s - (const char *) p);

        for (size_t j = 0; j < slen; j++) {
            unsigned char c = (unsigned char) s[j];

            if (s[j] == '\'') {
                break;
            }
            if (isdigit(c) || (s[j] == '.')) {
                str += s[j];
            }
        }
    }

    try {
        tempC = stof(str);
    } catch (const invalid_argument& e) {
    } catch (const out_of_range &e) {
    }

done:

    if (fd != -1) {
        close(fd);
    }

    return tempC;
}

string MeshPump::handleEnv(uint32_t node_num, string &message)
{
    stringstream ss;

    ss << HomeChat::handleEnv(node_num, message);
    if (!ss.str().empty()) {
        ss << " ";
    }

    ss << "temp_cpu=";
    ss <<  setprecision(3) << getCpuTempC();

    return ss.str();
}

string MeshPump::handleStatus(uint32_t node_num, string &message)
{
    stringstream ss;

    (void)(node_num);
    (void)(message);

    ss << "status: fish=" << (isFishPumpOn() ? "on" : "off")
       << " up=" << (isUpPumpOn() ? "on" : "off")
       << " up_cutoff=" << getUpPumpAutoCutoffSec() << "s";

    return ss.str();
}

string MeshPump::handleUnknown(uint32_t node_num, uint32_t dest, uint8_t channel, string &message)
{
    string reply;
    string first_word;

    (void)(node_num);
    (void)(dest);
    (void)(channel);
    (void)(message);

    first_word = message.substr(0, message.find(' '));
    toLowercase(first_word);
    message = message.substr(first_word.size());
    trimWhitespace(message);

    if (first_word == "led") {
        reply = handleLed(node_num, message);
    } else if (first_word == "pump") {
        reply = handlePump(node_num, message);
    } else if (first_word == "rollcall") {
        reply = handleRollcall(node_num, message);
    }

    return reply;
}

string MeshPump::handleRollcall(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);

    trimWhitespace(message);

    if (!message.empty()) {
        string target = message;
        string first_word = target.substr(0, target.find(' '));
        toLowercase(first_word);

        if (first_word != "all") {
            bool matches = false;

            if (_client != NULL) {
                uint32_t whoami = _client->whoami();
                char hexBuf1[16], hexBuf2[16], hexBuf3[16];
                snprintf(hexBuf1, sizeof(hexBuf1), "!%08x", (unsigned int)whoami);
                snprintf(hexBuf2, sizeof(hexBuf2), "0x%08x", (unsigned int)whoami);
                snprintf(hexBuf3, sizeof(hexBuf3), "%08x", (unsigned int)whoami);

                string myShortName = _client->lookupShortName(whoami);
                toLowercase(myShortName);
                string myLongName = _client->lookupLongName(whoami);
                toLowercase(myLongName);

                if (first_word == hexBuf1 ||
                    first_word == hexBuf2 ||
                    first_word == hexBuf3 ||
                    first_word == myShortName ||
                    first_word == myLongName ||
                    first_word == _client->whoamiString()) {
                    matches = true;
                } else {
                    uint32_t targetId = 0;
                    if (first_word.size() > 1 && first_word[0] == '!') {
                        targetId = (uint32_t)strtoul(first_word.c_str() + 1, NULL, 16);
                    } else if (first_word.rfind("0x", 0) == 0) {
                        targetId = (uint32_t)strtoul(first_word.c_str(), NULL, 16);
                    } else {
                        targetId = (uint32_t)strtoul(first_word.c_str(), NULL, 10);
                    }
                    if (targetId != 0 && targetId == whoami) {
                        matches = true;
                    }
                }
            }

            if (!matches) {
                return "";
            }
        }
    }

    reply = "rollcall: app=meshpump ver=";
    reply += MYPROJECT_VERSION_STRING;
    reply += " hw=linux caps=pump_fish,pump_up,led,env";

    return reply;
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

string MeshPump::handleLed(uint32_t node_num, string &message)
{
    stringstream ss;
    istringstream iss(message);
    vector<string> tokens;
    string token;
    string first_word;
    int y;

    while (getline(iss, token, ' ')) {
        tokens.push_back(token);
    }

    if (tokens.size() > 0) {
        first_word = tokens[0];
        toLowercase(first_word);
    }

    if ((first_word == "delay") && (tokens.size() == 2)) {
        try {
            int ms = stoi(tokens[1]);
            if (ms <= 0) {
                ss << "delay ms=" << tokens[1] << " is invalid!";
                goto done;
            }

            ledMatrix->setDelay((unsigned int) ms);
            ss << "set delay to " << ms << "ms";
            goto done;
        } catch (const invalid_argument &e) {
            ss << "delay ms=" << tokens[2] << " is invalid!";
            goto done;
        }
    } else if ((first_word == "sf") && (tokens.size() == 3) &&
               ((y = getArgY(tokens[1].c_str())) != -1)) {
        try {
            int sf = stoi(tokens[2]);
            if (sf < 1) {
                ss << "sf=" << tokens[2] << " is invalid!";
                goto done;
            }

            ledMatrix->setSlowdownFactor(y, (unsigned int) sf);
            ss << "set sf of row " << y << " to " << sf;
            goto done;
        } catch (const invalid_argument &e) {
            ss << "sf=" << tokens[2] << " is invalid!";
            goto done;
        }
    } else if (first_word == "blank") {
        ledMatrix->clear();
    } else if (first_word == "welcome") {
        ledMatrix->setWelcomeText();
    } else if ((y = getArgY(first_word.c_str())) != -1) {
        message = message.substr(first_word.size());
        trimWhitespace(message);
        ledMatrix->setText(y, message);
    } else {
        ss << "delay: " << to_string(ledMatrix->delay()) << "ms" << endl;
        for (unsigned int y = 0; y < MAX7219_Y_COUNT; y++) {
            ss << "row " << to_string(y) << ": ";
            ss << "ttl=" << to_string(ledMatrix->ttl(y)) << "s, ";
            ss << "sf=" << to_string(ledMatrix->slowdownFactor(y));
            if ((y + 1) != MAX7219_Y_COUNT) {
                ss << endl;
            }
        }
        goto done;
    }

    ss << "Led matrix updated for " << getDisplayName(node_num);

done:

    return ss.str();
}

string MeshPump::handlePump(uint32_t node_num, string &message)
{
    string reply;
    string first_word, second_word, third_word;
    bool isFish = false;
    bool isUp = false;
    bool onOff = false;
    unsigned int cutoff = 0;

    (void)(node_num);
    (void)(message);

    trimWhitespace(message);
    if (message.empty() || message == "status") {
        reply = string("pump: fish=") + (isFishPumpOn() ? "on" : "off") +
            " up=" + (isUpPumpOn() ? "on" : "off") +
            " cutoff=" + to_string(getUpPumpAutoCutoffSec()) + "s";
        return reply;
    }

    first_word = message.substr(0, message.find(' '));
    toLowercase(first_word);

    if ((first_word == "0") ||
        (first_word == "fish") ||
        (first_word == "fish-pump")) {
        isFish = true;
    } else if ((first_word == "1") ||
               (first_word == "up") ||
               (first_word == "up-pump")) {
        isUp = true;
    } else {
        reply = "no pump specified!";
        goto done;
    }

    message = message.substr(first_word.size());
    trimWhitespace(message);
    second_word = message.substr(0, message.find(' '));
    toLowercase(second_word);

    if (second_word == "on") {
        onOff = true;
    } else if (second_word == "off") {
        onOff = false;
    } else {
        reply = "no on/off specified!";
        goto done;
    }

    if (isUp && (onOff == true)) {
        message = message.substr(second_word.size());
        trimWhitespace(message);
        third_word = message.substr(0, message.find(' '));
        toLowercase(third_word);

        try {
            cutoff = stoi(third_word);
        } catch (const invalid_argument &e) {
            reply = "cutoff '" + third_word + "' argument is invalid!";
            goto done;
        }

        if (cutoff > MAX_UPPUMP_AUTO_CUTOFF_SEC) {
            reply = "cut-off of " + to_string(cutoff) +
                " seconds is too big!";
            goto done;
        }
    }

    if (isFish) {
        setFishPumpOnOff(onOff);
        reply = string("pump: fish=") + (onOff ? "on" : "off");
    } else if (isUp) {
        if (onOff == false) {
            setUpPumpOnOff(false);
            reply = "pump: up=off";
        } else {
            if (cutoff == 0) {
                setUpPumpOnOff(true);
                reply = "pump: up=on cutoff=" +
                    to_string(getUpPumpAutoCutoffSec()) + "s";
            } else {
                setUpPumpOnWithCutoffSec(cutoff);
                reply = "pump: up=on cutoff=" +
                    to_string(cutoff) + "s";
            }
        }
    }

done:

    return reply;
}

static inline int stdio_vprintf(const char *format, va_list ap)
{
    return vprintf(format, ap);
}

int MeshPump::vprintf(const char *format, va_list ap) const
{
    return stdio_vprintf(format, ap);
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

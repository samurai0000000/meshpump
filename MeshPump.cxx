/*
 * MeshPump.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <MeshPump.hxx>
#include <LedMatrix.hxx>

extern shared_ptr<LedMatrix> ledMatrix;

MeshPump::MeshPump()
    : MeshClient()
{
    setFishPumpOnOff(true);
    setUpPumpOnOff(false);
    setUpPumpAutoCutoffSec(10);
}

MeshPump::~MeshPump()
{

}

void MeshPump::join(void)
{
    MeshClient::join();
}

bool MeshPump::isFishPumpOn(void) const
{
    return _fishPump;
}

void MeshPump::setFishPumpOnOff(bool on)
{
    _fishPump = on;
}

bool MeshPump::isUpPumpOn(void) const
{
    return _upPump;
}

void MeshPump::setUpPumpOnOff(bool on)
{
    _upPump = on;
}

void MeshPump::setUpPumpOnWithCutoffSec(unsigned int seconds)
{
    (void)(seconds);
    _upPump = true;
}

unsigned int MeshPump::getUpPumpAutoCutoffSec(void) const
{
    return _upPumpAutoCutoffSec;
}

void MeshPump::setUpPumpAutoCutoffSec(unsigned int seconds)
{
    _upPumpAutoCutoffSec = seconds;
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

string MeshPump::handleUnknown(uint32_t node_num, string &message)
{
    string reply;
    string first_word;

    (void)(node_num);
    (void)(message);

    first_word = message.substr(0, message.find(' '));
    toLowercase(first_word);
    message = message.substr(first_word.size());
    trimWhitespace(message);

    if (first_word == "led") {
        reply = handleLed(node_num, message);
    } else if (first_word == "pump") {
        reply = handlePump(node_num, message);
    }

    return reply;
}

string MeshPump::handleStatus(uint32_t node_num, string &message)
{
    stringstream ss;

    (void)(node_num);
    (void)(message);

    ss << "fish-pump: " << (isFishPumpOn() ? "on" : "off") << endl;
    ss << "up-pump: " << (isUpPumpOn() ? "on" : "off") << endl;
    ss << "up-pump auto cutoff: " << getUpPumpAutoCutoffSec() << " seconds";

    return ss.str();
}

string MeshPump::handleLed(uint32_t node_num, string &message)
{
    string reply;
    string first_word;

    first_word = message.substr(0, message.find(' '));
    toLowercase(first_word);

    if (first_word == "3") {
        message = message.substr(first_word.size());
        trimWhitespace(message);
        ledMatrix->setText(3, message);
    } else if (first_word == "2") {
        message = message.substr(first_word.size());
        trimWhitespace(message);
        ledMatrix->setText(2, message);
    } else if (first_word == "1") {
        message = message.substr(first_word.size());
        trimWhitespace(message);
        ledMatrix->setText(1, message);
    } else if (first_word == "0") {
        message = message.substr(first_word.size());
        trimWhitespace(message);
        ledMatrix->setText(0, message);
    } else {
        ledMatrix->setText(0, message);
    }

    reply = "Led matrix updated for ";
    reply += getDisplayName(node_num);

    return reply;
}

string MeshPump::handlePump(uint32_t node_num, string &message)
{
    string reply;
    string first_word, second_word, third_word;
    bool isFish = false;
    bool isUp = false;
    bool onOff = false;
    unsigned int cutoff = 0;

    (void)(message);

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
        }

        if (cutoff > 120) {
            reply = "cut-off of " + to_string(cutoff) +
                " seconds is too big!";
            goto done;
        }
    }

    if (isFish) {
        setFishPumpOnOff(onOff);
        reply = "set fish-pump to ";
        reply += (onOff ? "on" : "off");
        reply += " by ";
        reply += getDisplayName(node_num);
    } else if (isUp) {
        if (onOff == false) {
            setUpPumpOnOff(false);
            reply = "set up-pump to off by " + getDisplayName(node_num);
        } else {
            if (cutoff == 0) {
                setUpPumpOnOff(true);
                reply = "set up-pump to on for " +
                    to_string(getUpPumpAutoCutoffSec()) +
                    " seconds by " +
                    getDisplayName(node_num);
            } else {
                setUpPumpOnWithCutoffSec(cutoff);
                reply = "set up-pump to on for " +
                    to_string(cutoff) +
                    " seconds by " +
                    getDisplayName(node_num);
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

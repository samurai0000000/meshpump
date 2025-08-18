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

    result = MeshNVM::loadNvm();

    return result;
}

bool MeshPump::saveNvm(void)
{
    bool result;

    result = MeshNVM::saveNvm();

    return result;
}

string MeshPump::handleUnknown(uint32_t node_num, string &message)
{
    string reply;
    string first_word;

    (void)(node_num);

    // get first word
    first_word = message.substr(0, message.find(' '));
    toLowercase(first_word);
    message = message.substr(first_word.size());
    trimWhitespace(message);

    if (first_word == "led") {
        reply = handleLed(node_num, message);
    }

    return reply;
}

string MeshPump::handleLed(uint32_t node_num, string &message)
{
    string reply;
    string first_word;

    // get first word
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

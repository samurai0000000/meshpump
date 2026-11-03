/*
 * test/TestMeshPumpFuzz.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <cstdarg>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <rapidcheck.h>
#include "MeshPump.hxx"
#include "MeshPumpShell.hxx"
#include "LedMatrix.hxx"
#include "LgpioShim.h"

shared_ptr<MeshPump> meshpump = NULL;
shared_ptr<LedMatrix> ledMatrix = NULL;

namespace {

class FuzzMeshPump : public MeshPump {
public:
    FuzzMeshPump()
        : MeshPump()
    {
    }

    using MeshPump::handleEnv;
    using MeshPump::handleStatus;
    using MeshPump::handleUnknown;
    using MeshPump::handleRollcall;
    using MeshPump::handleLed;
    using MeshPump::handlePump;
    using MeshPump::crontab;

    void setMyNode(uint32_t id, const char *shortName, const char *longName)
    {
        lock_guard<recursive_mutex> lock(_mutex);
        _myNodeInfo.my_node_num = id;

        meshtastic_NodeInfo node;
        memset(&node, 0, sizeof(node));
        node.num = id;
        node.has_user = true;
        snprintf(node.user.short_name, sizeof(node.user.short_name), "%s", shortName);
        snprintf(node.user.long_name, sizeof(node.user.long_name), "%s", longName);
        _nodeInfos[id] = node;
    }
};

class FuzzMeshPumpShell : public MeshPumpShell {
public:
    FuzzMeshPumpShell(shared_ptr<MeshClient> client = NULL)
        : MeshPumpShell(client)
    {
    }

    int printf(const char *format, ...) override
    {
        (void) format;
        return 0;
    }

    int vprintf(const char *format, va_list ap) override
    {
        (void) format;
        (void) ap;
        return 0;
    }

    int runCommand(const string &cmd)
    {
        vector<char> buf(cmd.begin(), cmd.end());
        buf.push_back('\0');
        return exec(buf.data());
    }
};

} // namespace

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    std::cout << "=== Running meshpump RapidCheck Fuzz Suite ===" << std::endl;
    bool all_passed = true;

    lgShimReset();
    ledMatrix = make_shared<LedMatrix>();
    shared_ptr<FuzzMeshPump> mp = make_shared<FuzzMeshPump>();
    mp->setClient(mp);
    mp->setMyNode(0x12345678U, "fuz", "FuzzNode");
    FuzzMeshPumpShell shell(mp);

    // Property 1: Arbitrary strings into MeshPump HomeChat handlers never throw or corrupt state
    std::cout << "\n[Property 1] Arbitrary strings into handlePump, handleLed, handleRollcall, handleUnknown"
              << std::endl;
    bool p1 = rc::check(
        "Property 1: MeshPump HomeChat handlers never throw on arbitrary strings",
        [&mp](const std::string &raw) {
            std::string m1 = raw;
            (void) mp->handlePump(1, m1);
            mp->setUpPumpOnOff(false);

            std::string m2 = raw;
            (void) mp->handleLed(1, m2);

            std::string m3 = raw;
            (void) mp->handleRollcall(1, m3);

            std::string m4 = raw;
            (void) mp->handleUnknown(1, 0xffffffffU, 0, m4);
            mp->setUpPumpOnOff(false);
        });
    all_passed &= p1;

    // Property 2: Structured & adversarial pump cutoff commands enforce 1..120s safety bounds
    std::cout << "\n[Property 2] Structured & adversarial pump commands enforce safety cutoff invariants"
              << std::endl;
    bool p2 = rc::check(
        "Property 2: handlePump enforces safety cutoff bounds and state consistency",
        [&mp]() {
            mp->setUpPumpOnOff(false);
            RC_ASSERT(!mp->isUpPumpOn());
            RC_ASSERT(lgShimGetGpioLevel(RELAY2_PIN) == 1);

            int mode = *rc::gen::inRange(0, 4);
            if (mode == 0) {
                // Valid cutoff 1..120
                unsigned int sec = *rc::gen::inRange<unsigned int>(
                    1, MAX_UPPUMP_AUTO_CUTOFF_SEC + 1);
                std::string cmd = "up on " + std::to_string(sec);
                std::string reply = mp->handlePump(1, cmd);
                RC_ASSERT(mp->isUpPumpOn());
                RC_ASSERT(lgShimGetGpioLevel(RELAY2_PIN) == 0);
                RC_ASSERT(reply.find("pump: up=on") != std::string::npos);

                std::string offCmd = "up off";
                (void) mp->handlePump(1, offCmd);
                RC_ASSERT(!mp->isUpPumpOn());
                RC_ASSERT(lgShimGetGpioLevel(RELAY2_PIN) == 1);
            } else if (mode == 1) {
                // Out-of-bounds positive cutoff > 120
                unsigned int sec = *rc::gen::inRange<unsigned int>(
                    MAX_UPPUMP_AUTO_CUTOFF_SEC + 1, 100000);
                std::string cmd = "up on " + std::to_string(sec);
                std::string reply = mp->handlePump(1, cmd);
                RC_ASSERT(!mp->isUpPumpOn());
                RC_ASSERT(lgShimGetGpioLevel(RELAY2_PIN) == 1);
                RC_ASSERT(reply.find("too big!") != std::string::npos);
            } else if (mode == 2) {
                // Zero or negative cutoff
                int badSec = *rc::gen::inRange<int>(-10000, 1);
                std::string cmd = "up on " + std::to_string(badSec);
                std::string reply = mp->handlePump(1, cmd);
                RC_ASSERT(!mp->isUpPumpOn());
                RC_ASSERT(lgShimGetGpioLevel(RELAY2_PIN) == 1);
                RC_ASSERT(reply.find("is invalid!") != std::string::npos);
            } else {
                // Arbitrary non-empty suffix
                std::string suffix = *rc::gen::arbitrary<std::string>();
                std::string cmd = "up on " + suffix;
                (void) mp->handlePump(1, cmd);
                mp->setUpPumpOnOff(false);
                RC_ASSERT(!mp->isUpPumpOn());
            }
        });
    all_passed &= p2;

    // Property 3: Adversarial led subcommands (delay, sf, row text) never OOB or throw
    std::cout << "\n[Property 3] Adversarial led subcommands never read OOB or throw"
              << std::endl;
    bool p3 = rc::check(
        "Property 3: handleLed subcommands never read OOB or throw",
        [&mp]() {
            std::string sub = *rc::gen::element<std::string>(
                "delay", "sf", "blank", "welcome", "0", "1", "2", "3", "4", "-1", "999999999999999999999");
            std::string arg1 = *rc::gen::arbitrary<std::string>();
            std::string arg2 = *rc::gen::arbitrary<std::string>();
            std::string cmd = sub + " " + arg1 + " " + arg2;
            (void) mp->handleLed(1, cmd);
        });
    all_passed &= p3;

    // Property 4: MeshPumpShell command parser resilience on arbitrary and structured inputs
    std::cout << "\n[Property 4] MeshPumpShell command parser resilience"
              << std::endl;
    bool p4 = rc::check(
        "Property 4: MeshPumpShell never throws on arbitrary commands",
        [&shell, &mp]() {
            std::string prefix = *rc::gen::element<std::string>(
                "pump ", "pump 0 ", "pump 1 ", "pump up on ", "led ", "led delay ", "led sf 0 ", "lighting ", "");
            std::string tail = *rc::gen::arbitrary<std::string>();
            (void) shell.runCommand(prefix + tail);
            mp->setUpPumpOnOff(false);
        });
    all_passed &= p4;

    mp->setClient(NULL);
    mp.reset();
    ledMatrix.reset();

    if (all_passed) {
        std::cout << "\n>>> ALL MESHPUMP RAPIDCHECK PROPERTIES PASSED SUCCESSFULLY! <<<"
                  << std::endl;
        return 0;
    } else {
        std::cerr << "\n>>> MESHPUMP RAPIDCHECK PROPERTY TEST FAILURE! <<<"
                  << std::endl;
        return 1;
    }
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

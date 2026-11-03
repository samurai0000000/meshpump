/*
 * test/TestMeshPump.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <sys/socket.h>
#include <unistd.h>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <string>
#include <memory>
#include <CppUTest/TestHarness.h>
#include "MeshPump.hxx"
#include "MeshPumpShell.hxx"
#include "LedMatrix.hxx"
#include "LgpioShim.h"

shared_ptr<MeshPump> meshpump = NULL;
shared_ptr<LedMatrix> ledMatrix = NULL;

namespace {

class TestableMeshPump : public MeshPump {
public:
    TestableMeshPump()
        : MeshPump(),
          _crontabCount(0)
    {
    }

    int _crontabCount;

    using MeshPump::crontab;
    using MeshPump::handleEnv;
    using MeshPump::handleStatus;
    using MeshPump::handleUnknown;
    using MeshPump::handleRollcall;
    using MeshPump::handleLed;
    using MeshPump::handlePump;
    using MeshPump::gotTextMessage;

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

    void setNode(uint32_t id, const char *shortName, const char *longName,
                 const meshtastic_User_public_key_t &pubkey)
    {
        lock_guard<recursive_mutex> lock(_mutex);
        meshtastic_NodeInfo node;
        memset(&node, 0, sizeof(node));
        node.num = id;
        node.has_user = true;
        node.user.public_key = pubkey;
        snprintf(node.user.short_name, sizeof(node.user.short_name), "%s", shortName);
        snprintf(node.user.long_name, sizeof(node.user.long_name), "%s", longName);
        _nodeInfos[id] = node;
    }

    void attachTestFd(int fd)
    {
        lock_guard<recursive_mutex> lock(_mutex);
        _mtc.fd = fd;
    }
};

class TestableMeshPumpShell : public MeshPumpShell {
public:
    TestableMeshPumpShell(shared_ptr<MeshClient> client = NULL)
        : MeshPumpShell(client)
    {
    }

    string output;

    void clearOutput(void)
    {
        output.clear();
    }

    int printf(const char *format, ...) override
    {
        char buf[1024];
        va_list args;
        va_start(args, format);
        int n = vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        if (n > 0) {
            output.append(buf, (size_t) n);
        }
        return n;
    }

    int vprintf(const char *format, va_list ap) override
    {
        char buf[1024];
        int n = vsnprintf(buf, sizeof(buf), format, ap);
        if (n > 0) {
            output.append(buf, (size_t) n);
        }
        return n;
    }

    int runCommand(const string &cmd)
    {
        char buf[512];
        strncpy(buf, cmd.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        return exec(buf);
    }

    shared_ptr<MeshShell> createChildInstance(void)
    {
        return newInstance();
    }
};

static string s_chat_log;

static int capture_chat_vprintf(void *ctx, const char *format, va_list ap)
{
    (void) ctx;
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    if (n > 0) {
        s_chat_log.append(buf, (size_t) n);
    }
    return n;
}

} // namespace

TEST_GROUP(MeshPumpTest)
{
    void setup()
    {
        lgShimReset();
        s_chat_log.clear();
        ledMatrix = make_shared<LedMatrix>();
    }

    void teardown()
    {
        if (meshpump) {
            meshpump->setClient(NULL);
            meshpump->setNvm(NULL);
            meshpump.reset();
        }
        ledMatrix.reset();
        lgShimReset();
        s_chat_log.clear();
    }
};

TEST(MeshPumpTest, InitialStateAndRelayPinLevels)
{
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();
    CHECK_TRUE(mp->isFishPumpOn());
    CHECK_FALSE(mp->isUpPumpOn());
    CHECK_EQUAL(10, mp->getUpPumpAutoCutoffSec());
    CHECK_FALSE(mp->isLightingOn());

    /* Active-low relays: ON -> 0, OFF -> 1 */
    CHECK_EQUAL(0, lgShimGetGpioLevel(RELAY1_PIN));
    CHECK_EQUAL(1, lgShimGetGpioLevel(RELAY2_PIN));
    CHECK_EQUAL(1, lgShimGetGpioLevel(RELAY3_PIN));

    mp->setFishPumpOnOff(false);
    CHECK_FALSE(mp->isFishPumpOn());
    CHECK_EQUAL(1, lgShimGetGpioLevel(RELAY1_PIN));

    mp->setFishPumpOnOff(true);
    CHECK_TRUE(mp->isFishPumpOn());
    CHECK_EQUAL(0, lgShimGetGpioLevel(RELAY1_PIN));

    mp->setLightingOnOff(true);
    CHECK_TRUE(mp->isLightingOn());
    CHECK_EQUAL(0, lgShimGetGpioLevel(RELAY3_PIN));

    mp->setLightingOnOff(false);
    CHECK_FALSE(mp->isLightingOn());
    CHECK_EQUAL(1, lgShimGetGpioLevel(RELAY3_PIN));
}

TEST(MeshPumpTest, UpPumpManualOnOffStateTransitionAndCancel)
{
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();
    CHECK_FALSE(mp->isUpPumpOn());

    mp->setUpPumpOnWithCutoffSec(2);
    CHECK_TRUE(mp->isUpPumpOn());
    CHECK_EQUAL(0, lgShimGetGpioLevel(RELAY2_PIN));

    /* Manual shutoff before timer expires must set _upPump = false and cancel timer */
    mp->setUpPumpOnOff(false);
    CHECK_FALSE(mp->isUpPumpOn());
    CHECK_EQUAL(1, lgShimGetGpioLevel(RELAY2_PIN));
}

TEST(MeshPumpTest, UpPumpAutoCutoffTimerExpiration)
{
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();
    mp->setUpPumpOnWithCutoffSec(1);
    CHECK_TRUE(mp->isUpPumpOn());
    CHECK_EQUAL(0, lgShimGetGpioLevel(RELAY2_PIN));

    /* Wait up to 1.5s for monotonic cutoff worker thread to turn off RELAY2_PIN */
    for (int i = 0; i < 30 && mp->isUpPumpOn(); i++) {
        usleep(50000);
    }

    CHECK_FALSE(mp->isUpPumpOn());
    CHECK_EQUAL(1, lgShimGetGpioLevel(RELAY2_PIN));
}

TEST(MeshPumpTest, UpPumpCutoffBoundariesZeroAndMax)
{
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();

    /* 0 seconds must be rejected by setUpPumpAutoCutoffSec and setUpPumpOnWithCutoffSec */
    mp->setUpPumpAutoCutoffSec(0);
    CHECK_EQUAL(10, mp->getUpPumpAutoCutoffSec());

    mp->setUpPumpAutoCutoffSec(1);
    CHECK_EQUAL(1, mp->getUpPumpAutoCutoffSec());

    mp->setUpPumpAutoCutoffSec(MAX_UPPUMP_AUTO_CUTOFF_SEC);
    CHECK_EQUAL(MAX_UPPUMP_AUTO_CUTOFF_SEC, mp->getUpPumpAutoCutoffSec());

    mp->setUpPumpAutoCutoffSec(MAX_UPPUMP_AUTO_CUTOFF_SEC + 1);
    CHECK_EQUAL(MAX_UPPUMP_AUTO_CUTOFF_SEC, mp->getUpPumpAutoCutoffSec());

    /* setUpPumpOnWithCutoffSec(0) and (121) must not energize the upper pump */
    mp->setUpPumpOnWithCutoffSec(0);
    CHECK_FALSE(mp->isUpPumpOn());
    CHECK_EQUAL(1, lgShimGetGpioLevel(RELAY2_PIN));

    mp->setUpPumpOnWithCutoffSec(MAX_UPPUMP_AUTO_CUTOFF_SEC + 1);
    CHECK_FALSE(mp->isUpPumpOn());
    CHECK_EQUAL(1, lgShimGetGpioLevel(RELAY2_PIN));
}

TEST(MeshPumpTest, HomeChatHandlePumpCommandsAndBoundaries)
{
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();
    string msg;
    string reply;

    /* Status queries */
    msg = "";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("pump: fish=on up=off cutoff=10s", reply.c_str());

    msg = "status";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("pump: fish=on up=off cutoff=10s", reply.c_str());

    /* Fish pump on/off */
    msg = "fish off";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("pump: fish=off", reply.c_str());
    CHECK_FALSE(mp->isFishPumpOn());

    msg = "0 on";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("pump: fish=on", reply.c_str());
    CHECK_TRUE(mp->isFishPumpOn());

    /* Upper pump on without 3rd argument (uses default 10s cutoff) */
    msg = "up on";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("pump: up=on cutoff=10s", reply.c_str());
    CHECK_TRUE(mp->isUpPumpOn());

    /* Upper pump off */
    msg = "up off";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("pump: up=off", reply.c_str());
    CHECK_FALSE(mp->isUpPumpOn());

    /* Upper pump on with explicit valid cutoff */
    msg = "1 on 15";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("pump: up=on cutoff=15s", reply.c_str());
    CHECK_TRUE(mp->isUpPumpOn());

    msg = "1 off";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("pump: up=off", reply.c_str());
    CHECK_FALSE(mp->isUpPumpOn());

    /* Boundary & fault cases */
    msg = "up on 0";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("cutoff '0' argument is invalid!", reply.c_str());
    CHECK_FALSE(mp->isUpPumpOn());

    msg = "up on -5";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("cutoff '-5' argument is invalid!", reply.c_str());
    CHECK_FALSE(mp->isUpPumpOn());

    msg = "up on 10abc";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("cutoff '10abc' argument is invalid!", reply.c_str());
    CHECK_FALSE(mp->isUpPumpOn());

    msg = "up on 999999999999999999999";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("cutoff '999999999999999999999' argument is invalid!", reply.c_str());
    CHECK_FALSE(mp->isUpPumpOn());

    msg = "up on 121";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("cut-off of 121 seconds is too big!", reply.c_str());
    CHECK_FALSE(mp->isUpPumpOn());

    msg = "unknown on";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("no pump specified!", reply.c_str());

    msg = "fish maybe";
    reply = mp->handlePump(1, msg);
    STRCMP_EQUAL("no on/off specified!", reply.c_str());
}

TEST(MeshPumpTest, HomeChatHandleLedCommandsAndFaults)
{
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();
    string msg;
    string reply;

    /* Valid delay */
    msg = "delay 40";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("set delay to 40ms", reply.c_str());
    CHECK_EQUAL(40, ledMatrix->delay());

    /* Multiple spaces between tokens */
    msg = "delay   55";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("set delay to 55ms", reply.c_str());
    CHECK_EQUAL(55, ledMatrix->delay());

    /* Invalid delay: non-numeric (previously read tokens[2] OOB!) */
    msg = "delay abc";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("delay ms=abc is invalid!", reply.c_str());

    /* Invalid delay: overflow (previously threw uncaught out_of_range) */
    msg = "delay 999999999999999999999";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("delay ms=999999999999999999999 is invalid!", reply.c_str());

    /* Invalid delay: 0 or negative or trailing chars */
    msg = "delay 0";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("delay ms=0 is invalid!", reply.c_str());

    msg = "delay 25ms";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("delay ms=25ms is invalid!", reply.c_str());

    /* Valid and invalid sf */
    msg = "sf 2 3";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("set sf of row 2 to 3", reply.c_str());
    CHECK_EQUAL(3, ledMatrix->slowdownFactor(2));

    msg = "sf 2 0";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("sf=0 is invalid!", reply.c_str());

    msg = "sf 2 bad";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("sf=bad is invalid!", reply.c_str());

    msg = "sf 2 999999999999999999999";
    reply = mp->handleLed(1, msg);
    STRCMP_EQUAL("sf=999999999999999999999 is invalid!", reply.c_str());

    /* Blank, welcome, row text, status */
    msg = "blank";
    reply = mp->handleLed(1, msg);
    CHECK_TRUE(reply.find("Led matrix updated") != string::npos);

    msg = "welcome";
    reply = mp->handleLed(1, msg);
    CHECK_TRUE(reply.find("Led matrix updated") != string::npos);

    msg = "1 HELLO WORLD";
    reply = mp->handleLed(1, msg);
    CHECK_TRUE(reply.find("Led matrix updated") != string::npos);

    msg = "";
    reply = mp->handleLed(1, msg);
    CHECK_TRUE(reply.find("delay: 55ms") != string::npos);
}

TEST(MeshPumpTest, NullLedMatrixResilience)
{
    ledMatrix.reset();
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();
    mp->setFishPumpOnOff(false);
    mp->setUpPumpOnOff(true);
    mp->setUpPumpOnOff(false);
    mp->setLightingOnOff(true);
    mp->setLightingOnOff(false);

    string msg = "delay 20";
    string reply = mp->handleLed(1, msg);
    CHECK_FALSE(reply.empty());
}

TEST(MeshPumpTest, CrontabHousekeepingAndScheduleTransitions)
{
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();

    /* NULL guard */
    mp->crontab(NULL);
    CHECK_FALSE(mp->isLightingOn());

    /* Nighttime hour (e.g. 20:00 -> shouldTurnOn = true) */
    struct tm tmNight;
    memset(&tmNight, 0, sizeof(tmNight));
    tmNight.tm_hour = 20;
    tmNight.tm_min = 0;
    mp->crontab(&tmNight);
    CHECK_TRUE(mp->isLightingOn());

    /* User manually overrides lighting off at 20:00; next minute 20:01 must NOT clobber override */
    mp->setLightingOnOff(false);
    CHECK_FALSE(mp->isLightingOn());
    tmNight.tm_min = 1;
    mp->crontab(&tmNight);
    CHECK_FALSE(mp->isLightingOn());

    /* Daytime transition (06:00 -> shouldTurnOn = false), then manual turn on at 06:01 */
    struct tm tmDay;
    memset(&tmDay, 0, sizeof(tmDay));
    tmDay.tm_hour = 6;
    tmDay.tm_min = 0;
    mp->crontab(&tmDay);
    CHECK_FALSE(mp->isLightingOn());

    mp->setLightingOnOff(true);
    CHECK_TRUE(mp->isLightingOn());
    tmDay.tm_min = 1;
    mp->crontab(&tmDay);
    CHECK_TRUE(mp->isLightingOn());

    /* Next night transition (19:00) followed by day transition (06:00) turns lighting off */
    tmNight.tm_hour = 19;
    mp->crontab(&tmNight);
    CHECK_TRUE(mp->isLightingOn());

    mp->crontab(&tmDay);
    CHECK_FALSE(mp->isLightingOn());
}

TEST(MeshPumpTest, RollcallAndIdentifyStrictTargetMatching)
{
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();
    mp->setClient(mp);
    mp->setMyNode(0x00001234U, "pmp1", "MeshPumpOne");

    string msg = "all";
    string reply = mp->handleRollcall(1, msg);
    CHECK_TRUE(reply.find("rollcall: app=meshpump") != string::npos);

    /* Exact short hex match (!1234) */
    msg = "!1234";
    reply = mp->handleRollcall(1, msg);
    CHECK_TRUE(reply.find("rollcall: app=meshpump") != string::npos);

    /* Partial hex with trailing garbage (!1234xyz) must be rejected */
    msg = "!1234xyz";
    reply = mp->handleRollcall(1, msg);
    STRCMP_EQUAL("", reply.c_str());

    msg = "0x1234xyz";
    reply = mp->handleRollcall(1, msg);
    STRCMP_EQUAL("", reply.c_str());

    /* Identify command via handleUnknown */
    msg = "identify !1234";
    reply = mp->handleUnknown(1, 0xffffffffU, 0, msg);
    CHECK_TRUE(reply.find("identify: app=meshpump") != string::npos);
}

TEST(MeshPumpTest, StatusEnvAndEndToEndGotTextMessage)
{
    int sv[2];
    CHECK_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));

    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();
    mp->setClient(mp);
    mp->setNvm(mp);
    mp->attachTestFd(sv[0]);
    mp->setMyNode(0x11223344U, "pmp", "PumpNode");

    meshtastic_User_public_key_t pubkey;
    memset(&pubkey, 0, sizeof(pubkey));
    pubkey.size = 32;
    memset(pubkey.bytes, 0x42, 32);
    mp->setNode(0x55667788U, "adm", "AdminNode", pubkey);
    mp->addAdmin(0x55667788U, pubkey);

    int dummy = 0;
    mp->addPrintfCallback(&dummy, capture_chat_vprintf);

    string msg = "";
    string st = mp->handleStatus(0x55667788U, msg);
    STRCMP_EQUAL("status: fish=on up=off up_cutoff=10s", st.c_str());

    string env = mp->handleEnv(0x55667788U, msg);
    CHECK_TRUE(env.find("temp_cpu=") != string::npos);

    meshtastic_MeshPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.from = 0x55667788U;
    pkt.to = 0x11223344U;
    pkt.channel = 0;

    s_chat_log.clear();
    mp->gotTextMessage(pkt, "pump up on 5");
    CHECK_TRUE(mp->isUpPumpOn());
    CHECK_TRUE(s_chat_log.find("pump: up=on cutoff=5s") != string::npos);

    s_chat_log.clear();
    mp->gotTextMessage(pkt, "pump up off");
    CHECK_FALSE(mp->isUpPumpOn());
    CHECK_TRUE(s_chat_log.find("pump: up=off") != string::npos);

    mp->setClient(NULL);
    mp->setNvm(NULL);
    close(sv[0]);
    close(sv[1]);
}

TEST(MeshPumpTest, MeshPumpShellCommandsAndDefectFixes)
{
    shared_ptr<TestableMeshPump> mp = make_shared<TestableMeshPump>();
    /* Intentionally keep global meshpump NULL to verify shell uses _client! */
    meshpump = NULL;

    TestableMeshPumpShell shell(mp);

    /* Verify newInstance propagates _client */
    shared_ptr<MeshShell> child = shell.createChildInstance();
    CHECK_TRUE(child != NULL);

    /* pump status */
    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("pump"));
    CHECK_TRUE(shell.output.find("fish-pump: on") != string::npos);
    CHECK_TRUE(shell.output.find("up-pump: off") != string::npos);

    /* pump 0 off / on (fish pump) */
    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("pump 0 off"));
    CHECK_FALSE(mp->isFishPumpOn());
    CHECK_TRUE(shell.output.find("set fish-pump to off") != string::npos);

    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("pump 0 on"));
    CHECK_TRUE(mp->isFishPumpOn());

    /* Defect 4: pump 1 on / off (upper pump using index "1") */
    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("pump 1 on 15"));
    CHECK_TRUE(mp->isUpPumpOn());
    CHECK_TRUE(shell.output.find("set up-pump to on for 15 seconds") != string::npos);

    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("pump 1 off"));
    CHECK_FALSE(mp->isUpPumpOn());
    CHECK_TRUE(shell.output.find("set up-pump to off") != string::npos);

    /* Boundary & invalid cutoffs in shell */
    shell.clearOutput();
    CHECK_EQUAL(-1, shell.runCommand("pump 1 on 0"));
    CHECK_FALSE(mp->isUpPumpOn());
    CHECK_TRUE(shell.output.find("cutoff '0' argument is invalid!") != string::npos);

    shell.clearOutput();
    CHECK_EQUAL(-1, shell.runCommand("pump 1 on -5"));
    CHECK_FALSE(mp->isUpPumpOn());

    shell.clearOutput();
    CHECK_EQUAL(-1, shell.runCommand("pump 1 on 999999999999999999999"));
    CHECK_FALSE(mp->isUpPumpOn());

    shell.clearOutput();
    CHECK_EQUAL(-1, shell.runCommand("pump 1 on 121"));
    CHECK_FALSE(mp->isUpPumpOn());
    CHECK_TRUE(shell.output.find("cut-off of 121 seconds is too big!") != string::npos);

    /* lighting on / off / status */
    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("lighting on"));
    CHECK_TRUE(mp->isLightingOn());

    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("lighting"));
    CHECK_TRUE(shell.output.find("lighting: on") != string::npos);

    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("lighting off"));
    CHECK_FALSE(mp->isLightingOn());

    /* led shell commands & overflow protection */
    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("led delay 30"));
    CHECK_EQUAL(30, ledMatrix->delay());

    shell.clearOutput();
    CHECK_EQUAL(-1, shell.runCommand("led delay 999999999999999999999"));
    CHECK_TRUE(shell.output.find("is invalid!") != string::npos);

    shell.clearOutput();
    CHECK_EQUAL(-1, shell.runCommand("led sf 0 999999999999999999999"));
    CHECK_TRUE(shell.output.find("is invalid!") != string::npos);

    /* system command */
    shell.clearOutput();
    CHECK_EQUAL(0, shell.runCommand("system"));
    CHECK_TRUE(shell.output.find("CPU temp:") != string::npos);
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

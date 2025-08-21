/*
 * MeshPump.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHPUMP_HXX
#define MESHPUMP_HXX

#include <LibMeshtastic.hxx>
#include <HomeChat.hxx>
#include <MeshNvm.hxx>

#define RELAY1_PIN  20
#define RELAY2_PIN  21
#define RELAY3_PIN  26

#define MAX_UPPUMP_AUTO_CUTOFF_SEC  120

using namespace std;

class MqttClient;

class MeshPump : public MeshClient, public MeshNvm, public HomeChat,
                 public enable_shared_from_this<MeshPump> {

public:

    MeshPump();
    ~MeshPump();

    void join(void);

    float getCpuTempC(void);

    bool isFishPumpOn(void) const;
    void setFishPumpOnOff(bool onOff);

    bool isUpPumpOn(void) const;
    void setUpPumpOnOff(bool onOff);
    void setUpPumpOnWithCutoffSec(unsigned int seconds);
    unsigned int getUpPumpAutoCutoffSec(void) const;
    void setUpPumpAutoCutoffSec(unsigned int seconds);

    bool isLightingOn(void) const;
    void setLightingOnOff(bool onOff);

protected:

    // Extend MeshClient

    virtual void gotTextMessage(const meshtastic_MeshPacket &packet,
                                const string &message);

    inline virtual HomeChat *getHomeChat(void) {
        return this;
    }

    virtual void crontab(const struct tm *now);

public:

    // Extend MeshNvm

    virtual bool loadNvm(void);
    virtual bool saveNvm(void);

protected:

    // Extend HomeChat

    virtual string handleEnv(uint32_t node_num, string &message);
    virtual string handleStatus(uint32_t node_num, string &message);
    virtual string handleUnknown(uint32_t node_num, string &message);
    virtual string handleLed(uint32_t node_num, string &message);
    virtual string handlePump(uint32_t node_num, string &message);
    virtual int vprintf(const char *format, va_list ap) const;

private:

    static void alarmHandler(int signum);

    bool _fishPump;
    bool _upPump;
    unsigned int _upPumpAutoCutoffSec;
    bool _lighting;

};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

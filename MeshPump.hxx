/*
 * MeshPump.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHPUMP_HXX
#define MESHPUMP_HXX

#include <LibMeshtastic.hxx>
#include <HomeChat.hxx>
#include <MeshNVM.hxx>

using namespace std;

class MqttClient;

class MeshPump : public MeshClient, public MeshNVM, public HomeChat,
                 public enable_shared_from_this<MeshPump> {

public:

    MeshPump();
    ~MeshPump();

    void join(void);

    bool isFishPumpOn(void) const;
    void setFishPumpOnOff(bool on);

    bool isUpPumpOn(void) const;
    void setUpPumpOnOff(bool on);
    unsigned int getUpPumpAutoCutoffSec(void) const;
    void setUpPumpAutoCutoffSec(unsigned int seconds);

protected:

    // Extend MeshClient

    virtual void gotTextMessage(const meshtastic_MeshPacket &packet,
                                const string &message);

    inline virtual HomeChat *getHomeChat(void) {
        return this;
    }

public:

    // Extend MeshNVM

    virtual bool loadNvm(void);
    virtual bool saveNvm(void);

protected:

    // Extend HomeChat

    virtual string handleUnknown(uint32_t node_num, string &message);
    virtual string handleLed(uint32_t node_num, string &message);
    virtual int vprintf(const char *format, va_list ap) const;

private:

    bool _fishPump;
    bool _upPump;
    unsigned int _upPumpAutoCutoffSec;

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

/*
 * MeshPump.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHPUMP_HXX
#define MESHPUMP_HXX

#include <LibMeshtastic.hxx>
#include <HomeChat.hxx>

using namespace std;

class MqttClient;

class MeshPump : public MeshClient, public HomeChat,
                 public enable_shared_from_this<MeshPump> {

public:

    MeshPump();
    ~MeshPump();

    void join(void);

protected:

    virtual void gotTextMessage(const meshtastic_MeshPacket &packet,
                                const string &message);
    virtual void gotRouting(const meshtastic_MeshPacket &packet,
                            const meshtastic_Routing &routing);
    virtual void gotTraceRoute(const meshtastic_MeshPacket &packet,
                               const meshtastic_RouteDiscovery &routeDiscovery);

protected:

    virtual int vprintf(const char *format, va_list ap) const;

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

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

MeshPump::MeshPump()
    : MeshClient()
{

}

MeshPump::~MeshPump()
{

}

void MeshPump::join(void)
{
    MeshClient::join();
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

void MeshPump::gotRouting(const meshtastic_MeshPacket &packet,
                         const meshtastic_Routing &routing)
{
    MeshClient::gotRouting(packet, routing);

    if ((routing.which_variant == meshtastic_Routing_error_reason_tag) &&
        (routing.error_reason == meshtastic_Routing_Error_NONE) &&
        (packet.from != packet.to)) {
        cout << "traceroute from " << getDisplayName(packet.from) << " -> ";
        cout << getDisplayName(packet.to)
             << "[" << packet.rx_snr << "dB]" << endl;
    }
}

void MeshPump::gotTraceRoute(const meshtastic_MeshPacket &packet,
                            const meshtastic_RouteDiscovery &routeDiscovery)
{
    MeshClient::gotTraceRoute(packet, routeDiscovery);
    if (!verbose()) {
        if ((routeDiscovery.route_count > 0) &&
            (routeDiscovery.route_back_count == 0)) {
            float rx_snr;
            cout << "traceroute from " << getDisplayName(packet.from)
                 << " -> ";
            for (unsigned int i = 0; i < routeDiscovery.route_count; i++) {
                if (i > 0) {
                    cout << " -> ";
                }
                cout << getDisplayName(routeDiscovery.route[i]);
                if (routeDiscovery.snr_towards[i] != INT8_MIN) {
                    rx_snr = routeDiscovery.snr_towards[i];
                    rx_snr /= 4.0;
                    cout << "[" << rx_snr << "dB]";
                } else {
                    cout << "[???dB]";
                }
            }
            rx_snr = packet.rx_snr;
            cout << " -> " << getDisplayName(packet.to)
                 << "[" << rx_snr << "dB]" << endl;
        }
    }
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

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
    MeshClient::gotTextMessage(packet, message);

    string reply;
    bool result;

    if (packet.to == whoami()) {
        cout << getDisplayName(packet.from) << ": "
             << message << endl;

        reply = lookupShortName(packet.from) + ", you said '" + message + "'!";
        if (reply.size() > 200) {
            reply = "oopsie daisie!";
        }

        result = textMessage(packet.from, packet.channel, reply);
        if (result == false) {
            cerr << "textMessage '" << reply << "' failed!" << endl;
        } else {
            cout << "my reply to " << getDisplayName(packet.from) << ": "
                 << reply << endl;
        }
    } else {
        cout << getDisplayName(packet.from) << " on #"
             << getChannelName(packet.channel) << ": "
             << message << endl;
        if ((packet.channel == 0) || (packet.channel == 1)) {
            string msg = message;
            transform(msg.begin(), msg.end(), msg.begin(),
                      [](unsigned char c) {
                          return tolower(c); });
            if (msg.find("hello") != string::npos) {
                reply = "greetings, " + lookupShortName(packet.from) + "!";
            } else if (msg.find(lookupShortName(whoami())) != string::npos) {
                reply = lookupShortName(packet.from) + ", you said '" +
                    message + "'!";
                if (reply.size() > 200) {
                    reply = "oopsie daisie!";
                }
            }

            if (!reply.empty()) {
                result = textMessage(0xffffffffU, packet.channel, reply);
                if (result == false) {
                    cerr << "textMessage '" << reply << "' failed!" << endl;
                } else {
                    cout << "my reply to " << getDisplayName(packet.from)
                         << ": " << reply << endl;
                }
            }
        }
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

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

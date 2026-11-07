// COOP OVER THE CLASSIC PROTOCOL: the window's coop campaign (server/campaign.hpp) served to others
// as a server, through the listen's door (client/listen.hpp's `CampaignDoor`). The classic
// endpoint (classic_server.hpp) serves the campaign's map; the window's player and the companions
// are its local seats; a client's `begin` enters the campaign (`campaign_join`, or nothing when
// the campaign carried him onto this map already) and his drop lets him go from it, so the campaign
// carries him from map to map with his parms as it carries the player. Each frame is the
// endpoint's (the network's moves, then the local seats'); a map change the frame asked for is
// the campaign's, the endpoint let go of the old map first and following it onto the next (every
// client told to sign on again). A coop game takes `most` players, the local seats counted.
//
// ge:summary Coop over the network: the window's coop campaign served on the classic protocol
// ge:keywords coop co-op coop-join campaign-door classic_coop campaign network join sv_port
// ge:keywords local-seat changelevel reconnect carried-players
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_coop_test.cpp
#pragma once

#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::net {

const client::CampaignDoor& classic_coop();

}  // namespace shooters_edge::net

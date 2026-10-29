// HOSTING FROM INSIDE A GAME: everything a game's HOST button needs beyond the node and the
// session, in one place, so a listen server in the player's own process and a dedicated server
// run the same calls. The game owns the node (accepting, `keep_foreign` set: hosting_start sets
// it) and the session host (session.hpp; `host_plays` for a listen server); hosting adds:
//
// - THE LAN: every probe on the discovery socket answered with the session's advert
//   (discovery.hpp), so a LAN-mate's browser finds the game.
// - PINGS: the same probe sent to the node's own port answered from the node's socket, so a
//   browser can time the game wherever it found it (directory.hpp).
// - THE ROUTER: with `open_port`, the node's port forwarded by whatever the home router speaks
//   (PCP, NAT-PMP, UPnP: portmap.hpp), renewed while the game runs and closed when it stops; a
//   router that speaks none of them is fine, the rendezvous's punch and relay still get players
//   in.
// - THE RENDEZVOUS: registered by the session's id (reach.hpp), so a client with the id (an
//   invite, or the list) is introduced and crosses the NATs, and introduced by the forwarded
//   port once there is one.
// - THE LIST: a LISTED game advertises its advert to the rendezvous the moment it changes (a
//   player joins, the game starts) and again every `advertise_us`, so every browser on the
//   internet sees it with its seats and phase; an advertise the rendezvous has not acknowledged
//   is sent again every `ask_us`. Stopping unlists it at once, asked again the same way.
//
// VISIBILITY: `lan` answers the LAN alone (no rendezvous, no router); `invite` registers (joined
// by the id the host hands out) but is never listed; `listed` is a public game.
//
//   hosting_start(h, config, node, session, lan, portmap_transport, now)
//   every frame, after node_update and session_host_update, before node_flush:
//     hosting_update(h, now)
//   hosting_stop(h, now), then hosting_update until `closed` (the forward taken down)
//
// The clock is the caller's: a hosted game over the simulated network replays to the bit (the
// `net-host` lab).
//
// ge:summary Hosting from inside a game: LAN answers, pings, the router's forward, register, list
// ge:keywords net hosting host-game listen-server dedicated-server self-host advertise listed
// ge:keywords invite-only lan-only visibility port-forward upnp rendezvous public-game ping
// ge:skill net
// ge:test tests/net_hosting_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "net/discovery.hpp"
#include "net/node.hpp"
#include "net/portmap.hpp"
#include "net/reach.hpp"
#include "net/session.hpp"

namespace ge::net {

enum class Visibility : uint8_t { lan, invite, listed };
const char* visibility_name(Visibility v);
bool visibility_of(std::string_view name, Visibility& out);

struct HostingConfig {
  uint32_t protocol = 0;
  std::string name = "game";  // the advert's (cut to kMaxAdvertName)
  Visibility visibility = Visibility::listed;
  bool locked = false;             // the advert says a password is needed (the game checks it)
  Address rendezvous;              // none: the LAN only, whatever the visibility
  uint64_t session = 0;            // the id at the rendezvous (0: drawn off `seed`)
  bool open_port = true;           // ask the router for a forward (needs the portmap transport)
  PortmapConfig portmap;           // the router (`internal_port` 0: the node's port)
  ReachConfig reach;               // its timings; the rendezvous and the session are filled in
  int64_t advertise_us = 5000000;  // a listed game advertises again this often
  int64_t ask_us = 500000;         // an advertise not acknowledged is sent again this often
  int unlist_tries = 8;            // a stop's unlist asked this many times at most
  uint64_t seed = 0;
};

enum class HostingState : uint8_t { idle, open, closing, closed };
const char* hosting_state_name(HostingState s);

struct HostingStats {
  uint64_t lan_answers = 0;  // LAN probes answered on the discovery socket
  uint64_t pings = 0;        // pings answered on the node's socket
  uint64_t advertised = 0;   // advertise messages sent
  uint64_t acked = 0;        // and acknowledged
};

struct Hosting {
  Node* node = nullptr;
  SessionHost* session = nullptr;
  Transport* lan = nullptr;     // the discovery socket (null: no LAN answers)
  Transport* router = nullptr;  // the portmap's socket (null: no forward asked)
  int64_t advertised_us = -1;   // when the advert last changed or was refreshed
  int64_t sent_us = -1;         // when the last advertise left
  int64_t started_us = 0;
  HostingStats stats;
  Advert advert;      // as it stands this frame
  Advert advertised;  // as last sent to the rendezvous
  ReachHost reach;
  HostingConfig config;
  Portmap portmap;
  uint32_t serial = 0;  // the last advertise's
  int unlist_left = 0;
  uint16_t forwarded = 0;  // the public port the router forwards, 0: none
  HostingState state = HostingState::idle;
  bool registering = false;  // the rendezvous is in use (invite or listed)
  bool mapping = false;      // the router was asked
  bool advertised_listed = false;
  bool acked = false;  // the rendezvous acknowledged the last advertise
};

void hosting_start(Hosting& h, const HostingConfig& config, Node& node, SessionHost& session,
                   Transport* lan, Transport* router, int64_t now_us);
void hosting_update(Hosting& h, int64_t now_us);
// The game ends: unlisted at once, the node closed to new players, and the forward taken down
// once the unlist is acknowledged (or its tries are spent).
void hosting_stop(Hosting& h, int64_t now_us);
// What an invite carries: the session's id at the rendezvous (0: a LAN-only game).
uint64_t hosting_invite(const Hosting& h);

}  // namespace ge::net

// THE ATTACKERS of a secure fake-traffic session (traffic.hpp's `TrafficAttack`, M17i): what the
// `net-link` lab's `--secure` sets on a host that holds connect tokens and its flood guards, each
// at a rate a second while everyone sends:
//
//   forged tokens   requests with a token made under another master key, from strangers
//   stolen tokens   requests with an honest client's whole token (its keys too), from strangers
//   replays         an honest client's sealed payloads sent again from its own address (spoofed)
//   forgeries       the same with their counter moved and a byte changed
//   a flood         padded requests without a token from ever-changing addresses
//   flooders        clients with good tokens of their own that send far more than a client does
//
// Each counts what it sent and every answer it got; a secure host answers none of them, and
// what it refused is in its NodeStats. Everything draws from the plan's seed.
//
// ge:summary The attackers of a secure traffic session: forged, stolen, replayed and flooded
// ge:keywords net attack attacker forged-token stolen-token replay forgery flood flooder security
// ge:skill net
// ge:test tests/net_traffic_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/prng.hpp"
#include "net/capture.hpp"
#include "net/node.hpp"
#include "net/sim_network.hpp"
#include "net/token.hpp"

namespace ge::net {

struct TrafficPlan;
struct AttackTally;

struct Attackers {
  struct Flooder {
    Transport transport;
    Node node;
  };
  const TrafficPlan* plan = nullptr;
  Key master{}, wrong{};  // the matchmaker's, and the forger's guess
  uint64_t host_id = 1;
  Address host;
  core::Rng rng{0};
  uint64_t sent[5] = {};         // forged tokens, stolen, replays, forgeries, the flood: so far
  Capture seen;                  // the honest client 0's datagrams this tick
  std::vector<uint8_t> payload;  // the last sealed payload client 0 sent
  std::vector<std::unique_ptr<Flooder>> flooders;
  uint64_t flood_sent = 0;
  uint32_t first_node = 0;  // the attackers' simulated nodes start here
};

// The plan's matchmaker: its master key and a token for client `id`, fresh random each.
Key traffic_master(uint32_t seed);
ConnectToken traffic_token(const Key& master, uint64_t client_id, int64_t expire_us,
                           core::Rng& rng);

// Ready the attackers on `net` against the host at `host` (nodes from `first_node` on).
void attackers_init(Attackers& a, const TrafficPlan& plan, SimNetwork& net, const Address& host,
                    uint32_t first_node);
// One tick: what is due by `now_us` of each attack, from the honest clients' `tokens`.
void attackers_tick(Attackers& a, SimNetwork& net, int64_t now_us,
                    const std::vector<ConnectToken>& tokens, AttackTally& tally);
// The flooders flush after everyone else.
void attackers_flush(Attackers& a, int64_t now_us);

}  // namespace ge::net

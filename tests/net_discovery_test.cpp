// engine/net/discovery.hpp: a probe broadcast over the simulated network finds every host on the
// discovery port and only those of its protocol, an advert never outgrows its probe, a stale
// nonce is ignored, and the same exchange over real loopback UDP.
//
// ge:summary Test: LAN discovery: broadcast probes, adverts, protocols, nonces, real UDP
// ge:keywords test net lan discovery broadcast probe advert udp loopback
// ge:skill net testing
#include "net/discovery.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"

using namespace ge::net;

namespace {

constexpr uint16_t kPort = 4500;

Advert advert(uint32_t protocol, uint16_t port, const char* name, int seated) {
  Advert a;
  a.protocol = protocol;
  a.port = port;
  a.seated = seated;
  a.max_players = 8;
  std::snprintf(a.name, sizeof a.name, "%s", name);
  return a;
}

}  // namespace

TEST(discovery_finds_every_host_of_its_protocol) {
  SimNetwork net = make_sim_network(1, LinkConditions{});
  const uint32_t game = protocol_id("disc", 1), other = protocol_id("other", 1);
  Transport hosts[3], client;
  for (uint32_t i = 0; i < 3; i++) open_sim_transport(hosts[i], net, sim_address(10 + i, kPort));
  open_sim_transport(client, net, sim_address(1, kPort + 1));
  const Advert ads[3] = {advert(game, 7000, "north", 3), advert(game, 7001, "south", 0),
                         advert(other, 7002, "elsewhere", 1)};
  CHECK(lan_probe(client, sim_broadcast(kPort), game, 99, 0));
  sim_advance(net, 1000);
  int answered = 0;
  for (int i = 0; i < 3; i++) answered += lan_answer(hosts[i], ads[i], 1000);
  CHECK(answered == 2);
  sim_advance(net, 2000);
  std::vector<Found> found;
  CHECK(lan_collect(client, game, 99, found, 2000) == 2);
  CHECK(found.size() == 2);
  CHECK(std::string(found[0].advert.name) == "north");
  CHECK(found[0].session == sim_address(10, 7000));
  CHECK(found[0].advert.seated == 3);
  CHECK(found[1].session == sim_address(11, 7001));
  // A second round updates the same entries.
  CHECK(lan_probe(client, sim_broadcast(kPort), game, 100, 3000));
  sim_advance(net, 4000);
  for (int i = 0; i < 3; i++) lan_answer(hosts[i], ads[i], 4000);
  sim_advance(net, 5000);
  CHECK(lan_collect(client, game, 99, found, 5000) == 0);  // answers to nonce 100, not 99
}

TEST(discovery_advert_never_outgrows_the_probe) {
  SimNetwork net = make_sim_network(1, LinkConditions{});
  const uint32_t game = protocol_id("disc", 1);
  Transport host, client;
  open_sim_transport(host, net, sim_address(10, kPort));
  open_sim_transport(client, net, sim_address(1, kPort + 1));
  // A short datagram that reads as a probe is not answered with a longer advert.
  uint8_t small[17] = {'g', 'e', 'L', 'N', 1};
  for (int i = 0; i < 4; i++) small[5 + i] = static_cast<uint8_t>(game >> (8 * i));
  CHECK(transport_send(client, sim_address(10, kPort), small, sizeof small, 0));
  sim_advance(net, 1000);
  CHECK(lan_answer(host, advert(game, 7000, "a long enough name", 1), 1000) == 0);
  CHECK(sizeof(Advert::name) == kMaxAdvertName + 1);
  CHECK(23 + kMaxAdvertName <= kProbeSize);
}

TEST(discovery_over_real_udp) {
  Transport host, client;
  std::string err;
  if (!open_udp_transport(host, loopback_v4(0), &err) ||
      !open_udp_transport(client, loopback_v4(0), &err)) {
    std::printf("skip: %s\n", err.c_str());
    return;
  }
  const uint32_t game = protocol_id("disc", 1);
  CHECK(lan_probe(client, host.local, game, 5, 0));
  int answered = 0;
  for (int i = 0; i < 100000 && !answered; i++)
    answered = lan_answer(host, advert(game, 7000, "real", 2), 0);
  CHECK(answered == 1);
  std::vector<Found> found;
  for (int i = 0; i < 100000 && found.empty(); i++) lan_collect(client, game, 5, found, 0);
  CHECK(found.size() == 1);
  CHECK(found.size() == 1 && found[0].advert.seated == 2 && found[0].session.port == 7000);
  close_transport(host);
  close_transport(client);
}

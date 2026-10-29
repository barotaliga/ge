// A HOSTING TRIAL: an evening of games hosted from inside a game and found in a browser, over the
// simulated network. The scene is fixed so its rows mean the same thing every run:
//
// - HOME: a listen server (the host plays) on a home LAN behind a NAT of `home_nat`, whose router
//   speaks `home_router` (portmap.hpp; 0: none, the NAT alone), listed; a FRIEND's browser on the
//   same LAN; it starts its game part way through and stops at the end.
// - AWAY: a player's browser on the internet behind a NAT of `away_nat`.
// - THE ARENAS: ten public dedicated servers, listed, some players seated on each, one locked
//   and one playing, so the list takes two pages and the filters have something to hide.
// - FRIENDS ONLY: a dedicated server hosted `invite`, never listed; AWAY joins it by its invite.
// - THE ATTIC: a listed host behind a symmetric NAT with no router: listed, but no stranger's
//   ping gets in, and only the relay reaches it.
// - THE BASEMENT: a LAN-only game on a LAN of its own, which nobody in the scene should see.
//
// The timeline: every host starts at 0; the browsers refresh at 6 s (a lossy link's router
// and rendezvous take a few seconds); once their refresh is done
// the friend joins HOME by the LAN and AWAY joins HOME, the attic and FRIENDS ONLY by the
// rendezvous; the
// joined chat; HOME starts at 14 s and both browsers refresh again at 15 s; HOME stops at 20 s
// and the browsers refresh a last time at 22 s. A refresh not done in 5 s is drawn as it stands. A
// pure function of its config: the same trial, the same hash.
//
// ge:summary A hosting trial: hosts found, pinged, joined, started and stopped
// ge:keywords net hosting trial server-browser directory lobby listen-server dedicated-server
// ge:keywords evening scene join chat invite unlist forward measure
// ge:skill net
// ge:test tests/net_hosting_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/conditions.hpp"
#include "net/directory.hpp"
#include "net/hosting.hpp"
#include "net/reach.hpp"
#include "net/rendezvous.hpp"
#include "net/sim_network.hpp"

namespace ge::net {

constexpr int kTrialOpen = -1;  // a NAT kind for "no NAT"

struct HostTrialConfig {
  LinkConditions link;
  uint32_t seed = 1;
  int home_nat = static_cast<int>(NatKind::port_restricted);  // or kTrialOpen
  uint8_t home_router = kPortmapAll;                          // what it speaks; 0: no router
  int away_nat = static_cast<int>(NatKind::port_restricted);  // or kTrialOpen
};

// One row of a browser's view, as it was drawn.
struct TrialRow {
  std::string name;
  int seated = 0, max_players = 0;
  SessionPhase phase = SessionPhase::lobby;
  bool locked = false;
  uint8_t from = 0;
  int ping_ms = -1;
};

struct TrialRefresh {
  int64_t at_us = 0, done_us = -1;  // done_us: when directory_done said so (-1: never)
  std::vector<TrialRow> rows;       // every entry, by ping
  DirectoryStats stats;
};

struct TrialJoin {
  std::string browser, host;
  bool lan = false;  // straight by the LAN address; else by the rendezvous
  ReachPath path = ReachPath::none;
  int64_t seated_ms = -1;   // from the join to the welcome; -1: never seated
  bool chat_heard = false;  // its line reached the host and the host's reached it
};

struct TrialBrowser {
  std::string name;
  std::vector<TrialRefresh> refreshes;  // the three
};

struct HostTrial {
  std::vector<TrialBrowser> browsers;  // friend, away
  std::vector<TrialJoin> joins;
  PortmapState home_portmap = PortmapState::idle;
  PortmapProto home_proto = PortmapProto::none;
  int64_t home_forward_ms = -1;      // when its router forwarded the port
  bool home_forward_closed = false;  // after the stop
  HostingStats home;
  int listed_at_server = 0;   // the rendezvous's listed sessions before the stop
  int listed_after_stop = 0;  // and 1.5 s after (HOME unlisted at once, a lost unlist asked again)
  bool home_listed_after_stop = false;
  RendezvousStats server;
  uint64_t hash = 0;  // every number above, folded
};

HostTrial run_host_trial(const HostTrialConfig& config);

}  // namespace ge::net

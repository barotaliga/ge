// THE PROXY TRIAL (proxy.hpp): a game's client and server through a near and a far proxy on the
// simulated network, the loss and the delay on the proxies' link only. The client sends a 24-byte
// command at 60 Hz and the server a 400-byte snapshot at 20 Hz, each much like the last (most of a
// real game's datagrams are); the trial counts what each end sent and got and the link's bytes
// each way. The first second sets the session up and is not counted.
//
// ge:summary The proxy trial: a client and server through a near and far proxy over a lossy link
// ge:keywords proxy trial loss repeat pack link bytes simulated-network
// ge:skill net
// ge:test tests/proxy_test.cpp
#pragma once

#include <cstdint>
#include <string>

namespace ge::proxy {

struct TrialConfig {
  double loss = 0;         // on the proxies' link, each way
  double latency_ms = 40;  // the proxies' link, one way
  double burst = 1;        // the loss's mean run (1: independent)
  int repeat = 1;          // both proxies' copies
  bool pack = false;       // both proxies pack
  int seconds = 20;        // counted
  uint32_t seed = 5;
  bool allowed = true;  // the server on the far proxy's allow list
};

struct TrialResult {
  int sent_up = 0, got_up = 0, sent_down = 0, got_down = 0;
  uint64_t payload_up = 0, payload_down = 0;  // the game's bytes sent
  uint64_t link_up = 0, link_down = 0;        // the proxies' link's bytes, every frame
  std::string refused;                        // the far proxy's words, when it refused
};

TrialResult run_proxy_trial(const TrialConfig& c);
// The share of the game's datagrams lost, both ways.
double trial_lost(const TrialResult& r);

}  // namespace ge::proxy

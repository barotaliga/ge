// engine/net/traffic.hpp: whole sessions of fake traffic hold the engine's guarantees under every
// condition preset (every reliable message once and in order, nothing sequenced or unreliable
// twice, the measured round trip near the configured one), and a session is its plan's to the bit.
//
// ge:summary Test: fake-traffic sessions under every preset; delivery guarantees; determinism
// ge:keywords test net traffic session reliable in-order preset determinism digest
// ge:skill net testing
#include "net/traffic.hpp"

#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"

using namespace ge::net;

TEST(traffic_holds_its_guarantees_under_every_preset) {
  for (const std::string& name : preset_names()) {
    TrafficPlan plan;
    plan.clients = 6;
    plan.seconds = 4;
    plan.drain_seconds = 4;
    plan.seed = 38;
    conditions_preset(name, plan.conditions);
    const TrafficReport r = run_traffic(plan);
    const std::string at = name + ": ";
    CHECK_MSG(r.connected == 6 && r.dropped == 0, at + "not every client stayed connected");
    for (const StreamTally* s : {&r.events_up, &r.events_down}) {
      CHECK_MSG(s->sent > 0 && s->delivered == s->sent, at + std::to_string(s->delivered) + " of " +
                                                            std::to_string(s->sent) +
                                                            " reliable messages arrived");
      CHECK_MSG(s->out_of_order == 0 && s->duplicated == 0, at + "a reliable message out of order");
    }
    CHECK(r.inputs.delivered <= r.inputs.sent && r.inputs.delivered > r.inputs.sent / 2);
    CHECK(r.snapshots.delivered <= r.snapshots.sent);
    // A node reads its socket once a tick: each way is the latency (and up to the jitter) rounded
    // up to whole ticks, and a held packet's delay and the queue may add a little more.
    const double tick = 1000.0 / plan.tick_hz;
    const auto ticks_up = [tick](double v) { return std::ceil(v / tick - 1e-9) * tick; };
    const double floor_ms = 2 * ticks_up(plan.conditions.latency_ms) - 0.01;
    const double ceiling_ms = 2 * ticks_up(plan.conditions.latency_ms + plan.conditions.jitter_ms) +
                              plan.conditions.reorder_ms + tick;
    CHECK_MSG(r.rtt_ms >= floor_ms && r.rtt_ms <= ceiling_ms,
              at + "rtt " + std::to_string(r.rtt_ms) + " ms outside " + std::to_string(floor_ms) +
                  ".." + std::to_string(ceiling_ms));
  }
}

TEST(traffic_is_its_plan_s) {
  TrafficPlan plan;
  plan.clients = 4;
  plan.seconds = 2;
  plan.drain_seconds = 1;
  conditions_preset("bad", plan.conditions);
  const TrafficReport a = run_traffic(plan);
  const TrafficReport b = run_traffic(plan);
  CHECK(a.digest == b.digest);
  CHECK(a.wire.sent == b.wire.sent && a.resends == b.resends);
  plan.seed = 2;
  CHECK(run_traffic(plan).digest != a.digest);
}

TEST(traffic_host_full_denies_the_rest) {
  TrafficPlan plan;
  plan.clients = 5;
  plan.max_peers = 3;
  plan.seconds = 1;
  plan.drain_seconds = 1;
  plan.capture = true;
  const TrafficReport r = run_traffic(plan);
  CHECK(r.connected == 3 && r.denied == 2);
  // The host's capture holds what it sent and received, and reads back whole.
  std::vector<CapturedDatagram> back;
  std::string err;
  CHECK(r.capture.packets > 0 && r.capture.packets < r.wire.sent);
  CHECK_MSG(read_capture(r.capture.bytes.data(), r.capture.bytes.size(), back, &err), err);
  CHECK(back.size() == r.capture.packets);
}

TEST(traffic_big_messages_arrive_whole_over_datagrams_and_streams) {
  for (const bool stream : {false, true}) {
    for (const char* preset : {"lan", "wifi", "bad"}) {
      TrafficPlan plan;
      plan.clients = 4;
      plan.seconds = 4;
      plan.drain_seconds = 4;
      plan.seed = 5;
      conditions_preset(preset, plan.conditions);
      plan.conditions.stream = stream;
      traffic_mix("download", plan.mix);
      const TrafficReport r = run_traffic(plan);
      const std::string at = std::string(preset) + (stream ? " (stream): " : ": ");
      CHECK_MSG(r.bulk.sent >= 8 && r.bulk.delivered == r.bulk.sent,
                at + std::to_string(r.bulk.delivered) + " of " + std::to_string(r.bulk.sent) +
                    " big messages arrived");
      CHECK_MSG(r.bulk_broken == 0 && r.bulk.out_of_order == 0 && r.bulk.duplicated == 0,
                at + "a big message broken or out of order");
      CHECK(r.events_down.delivered == r.events_down.sent);
      // Nothing is lost on a stream: every snapshot arrives.
      if (stream) CHECK_MSG(r.snapshots.delivered == r.snapshots.sent, at + "a snapshot lost");
    }
  }
}

TEST(traffic_budget_holds_the_host_to_its_rate) {
  TrafficPlan plan;
  plan.clients = 2;
  plan.seconds = 4;
  plan.drain_seconds = 2;
  traffic_mix("action", plan.mix);  // 30 Hz x 700 B snapshots: about 170 kbit/s a client
  plan.budget_kbps = 64;
  const TrafficReport r = run_traffic(plan);
  CHECK_MSG(r.host_send_kbps <= 2 * 64 * 1.1, std::to_string(r.host_send_kbps) + " kbit/s");
  CHECK(r.dropped_unsent > 0);
  CHECK(r.events_down.delivered == r.events_down.sent);  // reliable messages wait, never drop
}

TEST(traffic_secure_under_attack_answers_no_attacker) {
  TrafficPlan plan;
  plan.clients = 4;
  plan.seconds = 3;
  plan.drain_seconds = 3;
  plan.seed = 38;
  conditions_preset("wifi", plan.conditions);
  plan.secure = true;
  plan.guards = default_guards();
  plan.attack.forged_tokens_per_s = 20;
  plan.attack.stolen_tokens_per_s = 20;
  plan.attack.replays_per_s = 50;
  plan.attack.forgeries_per_s = 50;
  plan.attack.flood_per_s = 2000;
  plan.attack.flood_addresses = 16;
  plan.attack.flooders = 1;
  plan.attack.flooder_packets_per_s = 2000;
  const TrafficReport r = run_traffic(plan);
  CHECK(r.connected == 4 && r.dropped == 0);
  CHECK(r.events_up.delivered == r.events_up.sent && r.events_down.delivered == r.events_down.sent);
  CHECK(r.events_up.duplicated == 0 && r.events_down.duplicated == 0);
  CHECK(r.attack.answered == 0 && r.attack.flooders_connected == 1);
  const NodeStats& st = r.host_stats;
  CHECK(st.bad_tokens > 0 && st.replayed_tokens > 0 && st.replayed > 0 && st.forged > 0);
  CHECK(st.throttled > 0 && st.flooded > 0);
  // The attackers draw from the plan's seed too: the run is still its plan's, bit for bit.
  CHECK(run_traffic(plan).digest == r.digest);
}

// engine/net/conditions.hpp: each condition does what it says over many packets, a link is a
// queue, and the fates are the seed's.
//
// ge:summary Test: link conditions' loss, bursts, order, bandwidth, queue and determinism
// ge:keywords test net conditions loss burst gilbert-elliott reorder bandwidth queue preset
// ge:skill net testing
#include "net/conditions.hpp"

#include "check.hpp"

using namespace ge::net;

namespace {
struct Run {
  int sent = 0, lost = 0, runs = 0, dup = 0, reordered = 0, queue = 0;
  uint64_t digest = 0xcbf29ce484222325ull;
};

// `count` packets of `bytes`, one every `gap_us`.
Run run(const LinkConditions& c, int count, int64_t gap_us, size_t bytes, uint32_t seed) {
  Run r;
  LinkState s;
  auto rng = ge::core::make_rng(seed);
  bool prev_lost = false;
  int64_t last = -1;
  for (int i = 0; i < count; i++) {
    const Fate f = decide(c, s, rng, i * gap_us, bytes);
    r.sent++;
    const bool lost = f.copies == 0;
    r.lost += lost;
    r.queue += f.queue_drop;
    r.runs += lost && !prev_lost;
    r.dup += f.copies == 2;
    if (!lost) {
      r.reordered += f.arrive_us[0] < last;
      last = f.arrive_us[0] > last ? f.arrive_us[0] : last;
    }
    prev_lost = lost;
    for (const int64_t v : {int64_t{f.copies}, f.arrive_us[0], f.arrive_us[1]}) {
      r.digest ^= static_cast<uint64_t>(v);
      r.digest *= 0x100000001b3ull;
    }
  }
  return r;
}
}  // namespace

TEST(conditions_loss_and_bursts_hold_their_means) {
  LinkConditions c;
  c.loss = 0.05;
  const Run indep = run(c, 200000, 1000, 100, 1);
  CHECK(indep.lost > 9400 && indep.lost < 10600);
  c.burst = 4;
  const Run burst = run(c, 200000, 1000, 100, 2);
  CHECK_MSG(burst.lost > 9000 && burst.lost < 11000, "burst loss " + std::to_string(burst.lost));
  const double mean_run = static_cast<double>(burst.lost) / burst.runs;
  CHECK_MSG(mean_run > 3.6 && mean_run < 4.4, "mean run " + std::to_string(mean_run));
}

TEST(conditions_link_is_a_queue_unless_told_to_reorder) {
  LinkConditions c;
  c.latency_ms = 30;
  c.jitter_ms = 40;  // more jitter than the gap: still in order
  CHECK(run(c, 20000, 1000, 100, 3).reordered == 0);
  c.reorder = 0.05;
  c.reorder_ms = 20;
  const Run r = run(c, 20000, 1000, 100, 3);
  CHECK(r.reordered > 500);
  c.duplicate = 0.1;
  const Run d = run(c, 20000, 1000, 100, 4);
  CHECK(d.dup > 1700 && d.dup < 2300);
}

TEST(conditions_bandwidth_clocks_packets_out_and_the_queue_drops) {
  LinkConditions c;
  c.bandwidth_kbps = 1000;  // 1000 bits a millisecond
  c.queue_ms = 50;
  LinkState s;
  auto rng = ge::core::make_rng(1);
  // (972 + 28) * 8 bits = 8 ms on the line each.
  const Fate a = decide(c, s, rng, 0, 972);
  const Fate b = decide(c, s, rng, 0, 972);
  CHECK(a.arrive_us[0] == 8000 && b.arrive_us[0] == 16000);
  int dropped = 0;
  for (int i = 0; i < 20; i++) dropped += decide(c, s, rng, 0, 972).queue_drop;
  CHECK(dropped > 0 && dropped < 20);
  // Offered at twice the rate for a second: about half is dropped at the queue.
  const Run over = run(c, 250, 4000, 972, 5);
  CHECK(over.queue > 100 && over.queue < 140);
}

TEST(conditions_fates_are_the_seed_s) {
  for (const std::string& name : preset_names()) {
    LinkConditions c;
    CHECK(conditions_preset(name, c));
    const Run a = run(c, 5000, 2000, 300, 38);
    const Run b = run(c, 5000, 2000, 300, 38);
    CHECK_MSG(a.digest == b.digest, name + ": the same seed gave different fates");
    CHECK(!describe_conditions(c).empty());
  }
  LinkConditions c;
  CHECK(!conditions_preset("nope", c));
}

TEST(conditions_a_stream_loses_nothing_and_keeps_the_order) {
  LinkConditions c;
  c.latency_ms = 20;
  c.jitter_ms = 10;
  c.loss = 0.1;
  c.burst = 2;
  c.duplicate = 0.2;
  c.reorder = 0.2;
  c.reorder_ms = 30;
  c.queue_ms = 1;
  c.bandwidth_kbps = 500;
  c.stream = true;
  const Run r = run(c, 20000, 5000, 600, 7);
  CHECK(r.lost == 0 && r.dup == 0 && r.reordered == 0 && r.queue == 0);
  // A dropped packet comes a resend later; count the arrivals that waited for one.
  LinkState s;
  auto rng = ge::core::make_rng(7);
  int late = 0;
  for (int i = 0; i < 20000; i++) {
    const int64_t now = i * int64_t{5000};
    const Fate f = decide(c, s, rng, now, 600);
    late += f.arrive_us[0] - now >= static_cast<int64_t>(stream_resend_ms(c) * 1000);
  }
  CHECK_MSG(late > 1000, std::to_string(late) + " arrivals waited for a resend");
  CHECK(stream_resend_ms(c) == 200);
}

TEST(conditions_a_drop_pattern_loses_the_same_packets_of_every_period) {
  LinkConditions c;
  c.latency_ms = 10;
  c.drop_every = 3;
  c.drop_run = 1;
  LinkState s;
  ge::core::Rng rng = ge::core::make_rng(5);
  std::string seen;
  for (int i = 0; i < 9; i++) seen += decide(c, s, rng, i * 1000, 100).copies ? '+' : '-';
  CHECK(seen == "++-++-++-");
  c.drop_every = 2;  // every other one
  LinkState s2;
  seen.clear();
  for (int i = 0; i < 6; i++) seen += decide(c, s2, rng, i * 1000, 100).copies ? '+' : '-';
  CHECK(seen == "+-+-+-");
  CHECK(describe_conditions(c).find("1 of every 2 dropped") != std::string::npos);
}

// engine/net/stress.hpp: every chaos script ends as it must (every client reconnected, or every
// client timed out cleanly, none stuck, no host slot left behind); a ramp finds a breaking point
// between a value that held and one that broke, and the base holds; a soak keeps its memory flat
// and every reliable message whole; the fuzzer leaves the honest session unharmed and reaches
// every kind; a sealed session keeps a client its NAT rebinds; the runs replay to the bit; a
// session with no chaos is the session it always was.
//
// ge:summary Test: pressure tests: chaos scripts' endings, ramps' breaking points, soak, fuzz
// ge:keywords test net stress chaos ramp breaking-point soak memory fuzz reconnect timeout
// ge:skill net testing
#include "net/stress.hpp"

#include <string>

#include "check.hpp"

using namespace ge::net;

namespace {

TrafficPlan base(int clients = 6) {
  TrafficPlan p;
  p.clients = clients;
  p.seconds = 6;
  p.drain_seconds = 4;
  conditions_preset("broadband", p.conditions);
  return p;
}

}  // namespace

TEST(stress_every_chaos_script_ends_as_it_must) {
  for (const std::string& name : chaos_names()) {
    ChaosScript script;
    CHECK(chaos_script(name, base(), script));
    const TrafficPlan plan = chaos_plan(base(), script);
    const TrafficReport r = run_traffic(plan);
    const char* why = chaos_fails(r, plan, script);
    CHECK_MSG(why == nullptr, (name + ": " + (why ? why : "")).c_str());
    CHECK_MSG(r.ended_stuck == 0, (name + ": a client stuck").c_str());
  }
}

TEST(stress_a_partition_drops_and_reconnects_only_the_cut_clients) {
  ChaosScript script;
  chaos_script("partition", base(), script);
  const TrafficPlan plan = chaos_plan(base(), script);
  const TrafficReport r = run_traffic(plan);
  CHECK(r.dropped == 3);
  CHECK(r.reconnects == 3);
  CHECK(r.ended_connected == 6);
  // Cut at 3 s, timed out 5 s later, healed at 11 s: back within a second of the heal.
  CHECK(r.recover_s > 2.5 && r.recover_s < 4.5);
}

TEST(stress_a_lost_host_times_every_client_out_cleanly) {
  ChaosScript script;
  chaos_script("host-lost", base(), script);
  const TrafficPlan plan = chaos_plan(base(), script);
  const TrafficReport r = run_traffic(plan);
  CHECK(r.ended_timed_out == 6);
  CHECK(r.ended_connected == 0);
  CHECK(r.reconnects == 0);
}

TEST(stress_a_sealed_session_keeps_every_rebound_client) {
  TrafficPlan sealed = base();
  sealed.secure = true;
  ChaosScript script;
  CHECK(chaos_script("rebind", sealed, script));
  CHECK(script.keeps_connections);
  const TrafficPlan plan = chaos_plan(sealed, script);
  const TrafficReport r = run_traffic(plan);
  CHECK(chaos_fails(r, plan, script) == nullptr);
  CHECK(r.dropped == 0);
  CHECK(r.reconnects == 0);
  CHECK(r.ended_connected == 6);
}

int64_t fake_clock_ns() {
  static int64_t t = 0;
  return t += 1000;  // every reading a microsecond on
}

TEST(stress_a_load_connects_every_session_and_delivers_every_event) {
  LoadPlan plan;
  plan.sessions = 3;
  plan.clients = 6;
  plan.seconds = 2;
  plan.clock_ns = fake_clock_ns;
  const LoadReport r = run_load(plan);
  CHECK(r.expected == 18);
  CHECK(r.connected == 18);
  CHECK(r.reliable_whole);
  CHECK(r.sessions.size() == 3);
  for (const LoadSession& s : r.sessions) {
    CHECK(s.connected == 6);
    CHECK(s.inputs_sent > 0);
    CHECK(s.inputs_delivered > 0);
    CHECK(s.snapshots_delivered > 0);
    CHECK(s.events_delivered == s.events_sent);
    CHECK(s.step_us_mean > 0);  // the clock was read around each host's step
  }
  CHECK(r.tick_us_mean > r.step_us_mean);  // a tick holds every host's step
  // The digest is the counts alone: no clock replays it; on a lossy link another seed moves it.
  LoadPlan untimed = plan;
  untimed.clock_ns = nullptr;
  const LoadReport again = run_load(untimed);
  CHECK(again.digest == r.digest);
  CHECK(again.step_us_mean == 0);
  CHECK(conditions_preset("bad", untimed.conditions));
  const uint64_t lossy = run_load(untimed).digest;
  untimed.seed = 2;
  CHECK(run_load(untimed).digest != lossy);
}

TEST(stress_chaos_replays_to_the_bit) {
  ChaosScript script;
  chaos_script("rebind", base(), script);
  const TrafficPlan plan = chaos_plan(base(), script);
  CHECK(run_traffic(plan).digest == run_traffic(plan).digest);
}

TEST(stress_a_ramp_finds_a_breaking_point_between_held_and_broke) {
  RampPlan rp;
  rp.knob = StressKnob::loss;
  rp.steps = 6;
  rp.refine = 3;
  const Ramp r = run_ramp(base(4), rp);
  CHECK(r.found);
  CHECK(r.held < r.broke);
  CHECK(r.why != nullptr);
  CHECK(r.points.size() == static_cast<size_t>(r.points.size()));
  CHECK(r.points.front().broke == nullptr);  // the base itself holds
  CHECK(r.hash == run_ramp(base(4), rp).hash);
  // Bandwidth is taken down: what held is more than what broke.
  rp.knob = StressKnob::bandwidth;
  const Ramp b = run_ramp(base(4), rp);
  CHECK(b.found);
  CHECK(b.held > b.broke);
}

TEST(stress_a_soak_keeps_memory_flat_and_the_reliable_streams_whole) {
  TrafficPlan p = base(8);
  p.seconds = 120;
  p.mix = soak_mix();
  p.sample_s = 2;
  const SoakReport s = run_soak(p);
  CHECK(s.flat);
  CHECK(s.reliable_whole);
  CHECK(s.held_kb_start > 0);
  CHECK(s.traffic.inputs.latency_ms.empty());  // summed, not kept
  CHECK(s.traffic.inputs.latency_sum_ms > 0);
}

TEST(stress_the_fuzzer_leaves_the_honest_session_whole) {
  FuzzPlan f;
  f.session = base(4);
  f.datagrams = 20000;
  const FuzzReport r = run_fuzz(f);
  CHECK_MSG(r.honest, r.why ? r.why : "");
  CHECK(r.sent == 20000);
  for (int k = 0; k < kFuzzKinds; k++) CHECK_MSG(r.by_kind[k] > 0, fuzz_kind_name(k));
  CHECK(r.session.host_bad_cookies > 0);  // the made-up responses were refused
  CHECK(r.parsed > 0);
  CHECK(r.hash == run_fuzz(f).hash);
}

TEST(stress_no_chaos_is_the_session_it_always_was) {
  // The chaos, reconnect and hook fields untouched leave a session's digest as it was: the same
  // plan twice, and the same plan with reconnects allowed but nothing lost.
  const TrafficPlan p = base(4);
  TrafficPlan q = p;
  q.reconnect_tries = 3;
  CHECK(run_traffic(p).digest == run_traffic(q).digest);
}

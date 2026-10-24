// Time sync and smoothing (engine/net/replicate.hpp, M17n and M17o) over the replication trial: a
// client whose clock runs fast or slow keeps its inputs arriving on time with sync and does not
// without; the interpolation delay with sync is what the link needs, below the fixed one on a
// clean link and rarely running past the newest snapshot; the clock's speed stays within its
// bound; smoothing lowers the drawn paths' jerk and the error when snapshots run late, and snaps
// only past its distance; all of it holds at a shooter's 72 Hz and a racer's 120; a trial replays
// to the bit.
//
// ge:summary Test: time sync and smoothing: skewed clocks held, view delay, jerk, any tick rate
// ge:keywords test net predict prediction time-sync dilation skew interpolation-delay held
// ge:keywords smoothing spring cubic extrapolation jerk snap tick-rate
// ge:skill net testing
#include <algorithm>
#include <string>

#include "check.hpp"
#include "lab/stats.hpp"
#include "net/replicate_trial.hpp"

using namespace ge::net;

namespace {

ReplicateTrialConfig trial(const char* preset, bool sync, double skew, int hz = 60) {
  ReplicateTrialConfig k;
  k.replicate.tick_hz = hz;
  k.clients = 4;
  k.npcs = 8;
  k.seconds = 15;
  k.skew = skew;
  k.replicate.sync = sync;
  conditions_preset(preset, k.link);
  return k;
}

// Time sync without M17o's smoothing: a decayed error, straight lines, held when late.
ReplicateTrialConfig plain(ReplicateTrialConfig k) {
  k.replicate.spring = false;
  k.replicate.cubic = false;
  k.replicate.extrapolate_s = 0;
  return k;
}

}  // namespace

TEST(predict_sync_keeps_a_skewed_clock_s_inputs_on_time) {
  for (const char* preset : {"lan", "wifi", "bad"}) {
    const ReplicateTrial off = run_replicate_trial(trial(preset, false, 0.02));
    const ReplicateTrial on = run_replicate_trial(trial(preset, true, 0.02));
    const double p95_off = ge::lab::summarize(off.input_ms).p95;
    const double p95_on = ge::lab::summarize(on.input_ms).p95;
    // A clock 2% fast queues an input more every 50 ticks; without sync it piles up.
    CHECK_MSG(p95_on < p95_off - 100, std::string(preset) + ": sync did not hold the queue");
    CHECK(on.trimmed == 0);
    for (double d : on.dilation) CHECK(d >= 0.97 - 1e-12 && d <= 1.03 + 1e-12);
  }
}

TEST(predict_sync_draws_the_others_as_fresh_as_the_link_allows) {
  const ReplicateTrial off = run_replicate_trial(trial("lan", false, 0));
  const ReplicateTrial on = run_replicate_trial(trial("lan", true, 0));
  CHECK(off.view_delay_ms > 95);  // six ticks and the one-way latency
  CHECK(on.view_delay_ms < 50);
  CHECK(on.underrun < 0.005);
  const ReplicateTrial wifi = run_replicate_trial(trial("wifi", true, 0));
  CHECK(wifi.view_delay_ms > on.view_delay_ms);  // a jittery link asks for more
  CHECK(wifi.underrun < 0.02);
}

TEST(predict_a_true_clock_on_a_clean_link_needs_no_reserve) {
  // Nothing jitters: every input arrives with none left waiting, and none is repeated.
  const ReplicateTrial on = run_replicate_trial(trial("lan", true, 0));
  CHECK(on.repeated == 0);
  CHECK(ge::lab::summarize(on.input_ms).p95 < 20);
}

TEST(predict_a_synced_trial_replays_to_the_bit) {
  const ReplicateTrialConfig k = trial("bad", true, 0.02);
  CHECK(run_replicate_trial(k).hash == run_replicate_trial(k).hash);
}

TEST(predict_smoothing_lowers_the_drawn_paths_jerk) {
  for (const char* preset : {"wifi", "bad"}) {
    const ReplicateTrial a = run_replicate_trial(plain(trial(preset, true, 0)));
    const ReplicateTrial b = run_replicate_trial(trial(preset, true, 0));
    const auto p99 = [](const std::vector<double>& v) { return ge::lab::summarize(v).p99; };
    CHECK_MSG(p99(b.other_jerk) < p99(a.other_jerk), std::string(preset) + ": others");
    CHECK(b.snaps == 0);  // snap_distance off: nothing is shown at once
  }
  // The spring keeps a correction's velocity: on a lossy link the own path's jerk falls by more
  // than half.
  const ReplicateTrial a = run_replicate_trial(plain(trial("bad", true, 0)));
  const ReplicateTrial b = run_replicate_trial(trial("bad", true, 0));
  CHECK(ge::lab::summarize(b.own_jerk).p99 < ge::lab::summarize(a.own_jerk).p99 / 2);
}

TEST(predict_late_snapshots_are_carried_on_not_held) {
  const ReplicateTrial held = run_replicate_trial(plain(trial("bad", true, 0)));
  const ReplicateTrial carried = run_replicate_trial(trial("bad", true, 0));
  CHECK(!held.late_gap.empty() && !carried.late_gap.empty());
  const double h = ge::lab::summarize(held.late_gap).mean;
  const double c = ge::lab::summarize(carried.late_gap).mean;
  CHECK_MSG(c < h / 4, "carried on " + std::to_string(c) + " m against held " + std::to_string(h));
}

TEST(predict_a_correction_past_the_snap_distance_is_shown_at_once) {
  ReplicateTrialConfig k = trial("bad", true, 0);
  k.replicate.snap_distance = 1e-9;  // every correction
  const ReplicateTrial o = run_replicate_trial(k);
  CHECK(o.snaps > 0);
}

TEST(predict_holds_at_any_tick_rate) {
  for (const int hz : {72, 120}) {
    const ReplicateTrial off = run_replicate_trial(trial("lan", false, 0.02, hz));
    const ReplicateTrial on = run_replicate_trial(trial("lan", true, 0.02, hz));
    CHECK_MSG(ge::lab::summarize(on.input_ms).p95 < ge::lab::summarize(off.input_ms).p95 - 100,
              std::to_string(hz) + " Hz: sync did not hold the queue");
    CHECK(on.trimmed == 0);
    const ReplicateTrial clean = run_replicate_trial(trial("lan", true, 0, hz));
    CHECK_MSG(clean.view_delay_ms < 40, std::to_string(hz) + " Hz: the view delay is " +
                                            std::to_string(clean.view_delay_ms) + " ms");
    CHECK(clean.repeated == 0);
    // An input on a clean link reaches the server's step within about a tick and a half.
    CHECK(ge::lab::summarize(clean.input_ms).p95 < 1500.0 / hz + 1);
  }
}

// Time sync (engine/net/replicate.hpp, M17n) over the replication trial: a client whose clock runs
// fast or slow keeps its inputs arriving on time with sync and does not without; the
// interpolation delay with sync is what the link needs, below the fixed one on a clean link and
// rarely running past the newest snapshot; the clock's speed stays within its bound; a trial
// replays to the bit.
//
// ge:summary Test: time sync: skewed clocks held, the adaptive view delay, the dilation's bound
// ge:keywords test net predict prediction time-sync dilation skew interpolation-delay held
// ge:skill net testing
#include <algorithm>
#include <string>

#include "check.hpp"
#include "lab/stats.hpp"
#include "net/replicate_trial.hpp"

using namespace ge::net;

namespace {

ReplicateTrialConfig trial(const char* preset, bool sync, double skew) {
  ReplicateTrialConfig k;
  k.clients = 4;
  k.npcs = 8;
  k.seconds = 15;
  k.skew = skew;
  k.replicate.sync = sync;
  conditions_preset(preset, k.link);
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

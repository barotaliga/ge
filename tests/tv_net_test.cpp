// TV over engine/net (engine/tv/net_link.hpp, trial.hpp) on the simulated network: a source's
// node, a relay under it and viewers on both reach the same stream under every link preset, the
// whole of it from the keyframe block each started at (one that connects late starts later); a
// line said under the relay reaches every viewer; every node counts the whole tree; a delay holds
// every viewer back by about that much.
//
// ge:summary Tests TV over the simulated network: source, relay, viewers, every preset, chat, count
// ge:keywords test tv net relay tree simulated-network preset chat viewer-count delay lag
// ge:skill replay net testing
#include <algorithm>
#include <string>

#include "check.hpp"
#include "net/conditions.hpp"
#include "tv/trial.hpp"

namespace net = ge::net;
namespace tv = ge::tv;

TEST(every_viewer_gets_the_whole_stream_through_a_relay_under_every_preset) {
  for (const std::string& name : net::preset_names()) {
    tv::TrialConfig c;
    CHECK(net::conditions_preset(name, c.link));
    const tv::TrialResult r = tv::run_tv_trial(c);
    CHECK_MSG(r.all_whole, name + ": " + r.why);
    for (const tv::TrialViewer& v : r.viewers) {
      CHECK_MSG(v.chat == "viewer2: hello from the end of the line",
                name + ": heard '" + v.chat + "'");
      CHECK_MSG(v.told == 3, name + ": told " + std::to_string(v.told));
    }
    CHECK_MSG(r.counted == 3, name + ": counted " + std::to_string(r.counted));
  }
}

TEST(on_a_clean_link_every_viewer_starts_at_the_first_block) {
  tv::TrialConfig c;
  CHECK(net::conditions_preset("lan", c.link));
  const tv::TrialResult r = tv::run_tv_trial(c);
  CHECK(r.all_whole);
  for (const tv::TrialViewer& v : r.viewers) CHECK(v.first == 0 && v.blocks == c.blocks);
}

TEST(a_delay_holds_every_viewer_back_by_about_that_much) {
  tv::TrialConfig c;
  CHECK(net::conditions_preset("lan", c.link));
  c.block_ms = 1000;  // the match at its own speed
  c.blocks = 12;
  c.delay_ms = 3000;
  const tv::TrialResult r = tv::run_tv_trial(c);
  CHECK_MSG(r.all_whole, r.why);
  for (const tv::TrialViewer& v : r.viewers) {
    CHECK(!v.lag_ms.empty());
    const double least = *std::min_element(v.lag_ms.begin(), v.lag_ms.end());
    CHECK_MSG(least >= 2900 && least < 3500, "least lag " + std::to_string(least));
  }
}

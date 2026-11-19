// engine/server/skew: the move time a client asks for against real time over a window; a fast
// clock logged past sv_speedwarn and dropped past sv_speedkick; lost packets' commands counted.
//
// ge:summary Test: sv_speedwarn and sv_speedkick on an honest, a fast and a lossy client
// ge:keywords test sv_speedwarn sv_speedkick sv_speedwindow time-skew skew_packet skew_check
// ge:skill net testing
#include "check.hpp"
#include "server/skew.hpp"

using namespace ge::server;

namespace {

// A client sending a packet of three 13 ms commands every 13 ms of its own clock, `speed` times
// as fast as the server's, every `every`-th packet arriving; the check after 16 s of real time.
Skew play(double speed, int every, double& ratio) {
  SkewWatch w;
  const SkewRule rule{16, 1.05, 1.15};
  const int msec[3] = {13, 13, 13};
  uint32_t seq = 0;
  Skew got = Skew::fine;
  for (int n = 0; n * 0.013 / speed < 16.5; n++) {
    const double t = n * 0.013 / speed;
    if (++seq % static_cast<uint32_t>(every) != 0) continue;
    skew_packet(w, t, seq, msec);
    const Skew s = skew_check(w, t, rule, ratio);
    if (s != Skew::fine || ratio > 0) {
      got = s;
      if (ratio > 0) break;
    }
  }
  return got;
}

}  // namespace

TEST(a_fast_clock_is_logged_then_dropped) {
  double ratio = 0;
  CHECK(play(1.0, 1, ratio) == Skew::fine);
  CHECK_MSG(ratio > 0.98 && ratio < 1.02, std::to_string(ratio));
  ratio = 0;
  CHECK(play(1.1, 1, ratio) == Skew::warn);
  ratio = 0;
  CHECK(play(1.3, 1, ratio) == Skew::kick);
  ratio = 0;
  CHECK(play(1.0, 2, ratio) == Skew::fine);  // every other packet lost: the next carries both
  CHECK_MSG(ratio > 0.98 && ratio < 1.02, std::to_string(ratio));
}

TEST(the_settings_are_read_and_refused_out_of_range) {
  ServerConfig c;
  c.rules["sv_speedkick"] = "1.15";
  SkewRule r;
  std::string error;
  CHECK(skew_settings(c, r, error) && r.kick == 1.15 && r.window_s == 16);
  c.rules["sv_speedwindow"] = "0";
  CHECK(!skew_settings(c, r, error) && error.find("sv_speedwindow") != std::string::npos);
}

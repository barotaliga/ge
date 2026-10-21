// engine/loop/run_clock.hpp against tests/golden/loop_run_clock.txt. Per clock: a header row
//   hz max_frame n_ops, then n_ops rows of  op arg steps alpha dropped paused  (op 0 = frame(arg),
//   1 = pause, 2 = resume).
//
// ge:summary Golden test: loop/run_clock, bit for bit
// ge:keywords test golden run-clock
// ge:skill testing
#include "loop/run_clock.hpp"

#include <limits>

#include "golden.hpp"

using ge::test::as_double;

TEST(run_clock_matches_golden_bit_for_bit) {
  const auto rows = ge::test::golden_rows("tests/golden/loop_run_clock.txt");
  size_t at = 0;
  int clocks = 0;
  while (at < rows.size()) {
    const auto& head = rows[at++];
    const double hz = as_double(head[0]);
    const double max_frame = as_double(head[1]);
    const size_t ops = static_cast<size_t>(as_double(head[2]));
    ge::loop::RunClock clock = max_frame == ge::loop::kMaxFrameSeconds
                                   ? ge::loop::create_run_clock(hz)
                                   : ge::loop::create_run_clock(hz, max_frame);
    for (size_t i = 0; i < ops; i++) {
      const auto& row = rows[at++];
      const int op = static_cast<int>(as_double(row[0]));
      int steps = 0;
      if (op == 0) steps = ge::loop::frame(clock, as_double(row[1]));
      if (op == 1) ge::loop::pause(clock);
      if (op == 2) ge::loop::resume(clock);
      CHECK_BITS(static_cast<double>(steps), as_double(row[2]));
      CHECK_BITS(ge::loop::alpha(clock), as_double(row[3]));
      CHECK_BITS(clock.dropped, as_double(row[4]));
      CHECK_BITS(clock.paused ? 1.0 : 0.0, as_double(row[5]));
    }
    clocks++;
  }
  CHECK(clocks == 4);
}

// A NaN frame poisons the accumulator: that frame and every frame after take 0 steps.
TEST(nan_frame_takes_no_steps) {
  ge::loop::RunClock clock = ge::loop::create_run_clock(120.0);
  CHECK(ge::loop::frame(clock, std::numeric_limits<double>::quiet_NaN()) == 0);
  CHECK(ge::loop::frame(clock, 1.0 / 60.0) == 0);
  CHECK(ge::loop::alpha(clock) != ge::loop::alpha(clock));
  ge::loop::resume(clock);
  CHECK(ge::loop::frame(clock, 1.0 / 60.0) == 2);
}

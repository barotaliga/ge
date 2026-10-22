// engine/core/output's channels: names registered once, the log spec parsed whole or not at all,
// each channel's verbosity deciding what is kept, the step stamped on every line, and the dump's
// line format.
//
// ge:summary Test: core/output's channels, log spec, verbosity, step stamp and line format
// ge:keywords test output log channel verbosity configure log-spec set_step format_line
// ge:skill testing debug
#include <string>
#include <vector>

#include "check.hpp"
#include "core/output.hpp"

namespace out = ge::core::output;

namespace {

std::vector<std::string> lines;

void collect(const out::LogLine& line, void*) { lines.push_back(out::format_line(line)); }

}  // namespace

TEST(channels_register_once_and_engine_is_first) {
  CHECK(out::channel_name(out::kEngine) == "engine");
  const out::Channel a = out::channel("test-a");
  CHECK(a != out::kEngine);
  CHECK(out::channel("test-a") == a);
  CHECK(out::channel_name(a) == "test-a");
  CHECK(out::channel("Not A Name") == out::kEngine);  // logs on engine, warns once
}

TEST(verbosity_decides_what_is_kept) {
  CHECK(out::configure("").empty());
  const out::Channel a = out::channel("test-b");
  CHECK(out::enabled(a, out::Level::info));
  CHECK(!out::enabled(a, out::Level::debug));
  CHECK(out::configure("test-b=trace,*=warn").empty());
  CHECK(out::enabled(a, out::Level::trace));
  CHECK(!out::enabled(out::kEngine, out::Level::info));
  CHECK(out::enabled(out::kEngine, out::Level::warn));
  CHECK(out::configure("debug").empty());  // a bare verbosity is the rest's
  CHECK(out::enabled(a, out::Level::debug));
  CHECK(!out::enabled(a, out::Level::trace));
  // A channel named before it is first used keeps its setting.
  CHECK(out::configure("test-later=off").empty());
  CHECK(!out::enabled(out::channel("test-later"), out::Level::error));
  CHECK(out::configure("").empty());
}

TEST(a_bad_spec_changes_nothing_and_says_how_to_write_one) {
  CHECK(out::configure("test-c=debug").empty());
  const out::Channel c = out::channel("test-c");
  const std::string why = out::configure("test-c=info,test-d=loud");
  CHECK(why.find("test-d=loud") != std::string::npos);
  CHECK(why.find("docs/debugging.md") != std::string::npos);
  CHECK(out::verbosity(c) == out::Verbosity::debug);
  CHECK(!out::configure("Bad=debug").empty());
  CHECK(out::configure("").empty());
}

TEST(lines_carry_their_step_and_channel) {
  CHECK(out::configure("test-e=debug").empty());
  const out::Channel e = out::channel("test-e");
  lines.clear();
  out::set_output_sink(collect);
  lines.clear();  // the replayed buffer
  out::set_step(1234);
  out::log_on(e, out::Level::debug, "gate 3");
  out::log_on(e, out::Level::trace, "dropped");
  out::set_step(-1);
  out::info("outside");
  out::set_output_sink(nullptr);
  CHECK(lines.size() == 2);
  CHECK(lines.size() == 2 && lines[0] == "[1234] debug test-e: gate 3");
  CHECK(lines.size() == 2 && lines[1] == "[-] info engine: outside");
  CHECK(out::configure("").empty());
}

TEST(the_debug_switch_leaves_named_channels_alone) {
  CHECK(out::configure("test-f=warn").empty());
  out::set_debug_enabled(true);
  CHECK(out::debug_enabled());
  CHECK(!out::enabled(out::channel("test-f"), out::Level::info));
  out::set_debug_enabled(false);
  CHECK(!out::debug_enabled());
  CHECK(out::configure("").empty());
}

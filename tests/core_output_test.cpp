// engine/core/output against tests/golden/core_output.txt: the transcript of a fixed script of
// calls. This test runs the SAME script (keep it in step with the golden's generator) and must
// print the same
// lines: what each sink received, in order, and the buffer's contents at two points.
//
// ge:summary Golden test: core/output's transcript, bit for bit
// ge:keywords test golden output log
// ge:skill testing
#include "core/output.hpp"

#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"

namespace out = ge::core::output;

namespace {

struct Transcript {
  std::vector<std::string> lines;
};

template <int N>
void sink_to(const out::LogLine& line, void* user) {
  auto* t = static_cast<Transcript*>(user);
  t->lines.push_back("sink" + std::to_string(N) + " " + std::string(out::level_name(line.level)) +
                     " " + line.message);
}

void dump_recent(Transcript& t) {
  const auto recent = out::recent_logs();
  t.lines.push_back("recent " + std::to_string(recent.size()));
  for (const auto& line : recent) {
    t.lines.push_back("recent " + std::string(out::level_name(line.level)) + " " + line.message);
  }
}

}  // namespace

TEST(output_transcript_matches_golden) {
  Transcript t;
  for (int i = 0; i < 5; i++) out::status("boot " + std::to_string(i));
  out::debug("dropped before any sink");
  out::set_output_sink(sink_to<1>, &t);
  out::info("info line");
  out::warn("warn line");
  out::error("error line");
  out::header("Level");
  out::debug("dropped while debug is off");
  out::set_debug_enabled(true);
  out::debug("kept while debug is on");
  out::set_debug_enabled(false);
  out::debug("dropped again");
  dump_recent(t);
  for (int i = 0; i < 250; i++) out::status("fill " + std::to_string(i));
  out::set_output_sink(nullptr);
  out::warn("emitted with no sink");
  out::set_output_sink(sink_to<2>, &t);
  dump_recent(t);
  out::set_output_sink(nullptr);

  std::ifstream in(ge::test::root_path("tests/golden/core_output.txt"));
  std::vector<std::string> want;
  for (std::string line; std::getline(in, line);) want.push_back(line);
  CHECK(t.lines.size() == want.size());
  for (size_t i = 0; i < want.size() && i < t.lines.size(); i++) {
    if (t.lines[i] != want[i]) {
      ge::test::fail(__FILE__, __LINE__,
                     "transcript line " + std::to_string(i + 1) + ": got \"" + t.lines[i] +
                         "\" want \"" + want[i] +
                         "\". engine/core/output.cpp has drifted from the golden transcript.");
      break;
    }
  }
}

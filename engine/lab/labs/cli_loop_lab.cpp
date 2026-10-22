// LAB `cli-loop`: the ge command line as an agent meets it, measured. It scaffolds a game repo in
// a scratch directory and runs the loop's commands that need no compiler (help, help --json,
// new, add system, add lab, add test, use, doctor, find, explain) through the same code the ge
// executable runs, printing each answer as text the way an agent reads it. For each command it
// reports the answer's lines and bytes and how long it took, and it fails a command whose text
// answer outgrows its budget: an agent pays for every line ge prints. Only the catalog's shape
// (its commands and flags) is in the digest: the answers carry the version, name paths and read
// the engine's tags, which releases and changes move, so they are measured (printed and compared
// with --compare, never hashed).
// Building and testing a scaffolded repo for real is the packaging workflow's `scaffold` job.
//
// ge:summary Lab: the ge commands of an agent's loop run on a scaffolded repo; answer sizes, times
// ge:keywords lab cli ge loop output-size budget agent tokens help json new add use doctor find
// ge:keywords explain bench
// ge:skill agent-interface labs
// ge:test tests/cli_commands_test.cpp
#include <chrono>
#include <sstream>
#include <string>
#include <vector>

#include "cli/add.hpp"
#include "cli/commands.hpp"
#include "cli/doctor.hpp"
#include "cli/index.hpp"
#include "cli/outcome.hpp"
#include "cli/scaffold.hpp"
#include "lab/lab.hpp"

namespace {

using namespace ge::cli;
using ge::lab::Det;

struct Answer {
  std::string text;
  double ms;
};

template <typename F>
Answer timed(std::string_view command, F&& run) {
  const auto t0 = std::chrono::steady_clock::now();
  const Outcome o = run();
  const auto t1 = std::chrono::steady_clock::now();
  std::ostringstream out;
  emit(command, o, false, out);
  return {out.str(), std::chrono::duration<double, std::milli>(t1 - t0).count()};
}

int lines_of(const std::string& s) {
  int n = 0;
  for (const char c : s) n += c == '\n' ? 1 : 0;
  return n;
}

void run(ge::lab::Lab& lab) {
  auto record = [&](const std::string& key, const std::string& command, const Answer& a, int budget,
                    Det det) {
    const int lines = lines_of(a.text);
    lab.metric(key + ".lines", lines, "count", det);
    lab.metric(key + ".bytes", static_cast<double>(a.text.size()), "count", det);
    lab.metric(key + ".time", a.ms, "ms", Det::measured);
    if (lines > budget) {
      lab.fail(command + " answers in " + std::to_string(lines) + " lines, over its budget of " +
                   std::to_string(budget),
               "next: shorten its text answer (outcome.hpp), or raise the budget in "
               "engine/lab/labs/cli_loop_lab.cpp and say why in the PR");
    }
  };

  // The catalog: its shape is in the digest. The help's sizes carry the engine's version, which
  // every release moves, so they are measured.
  int flags = 0;
  for (const CommandSpec& c : commands()) flags += static_cast<int>(c.flags.size());
  lab.metric("catalog.commands", static_cast<double>(commands().size()), "count");
  lab.metric("catalog.flags", flags, "count");
  Outcome help;
  help.text = help_text(nullptr);
  record("help", "ge help", timed("ge help", [&] { return help; }), 40, Det::measured);
  Outcome help_json_answer;
  help_json_answer.result = help_json(nullptr);
  std::ostringstream json;
  emit("ge help", help_json_answer, true, json);
  lab.metric("help-json.bytes", static_cast<double>(json.str().size()), "count", Det::measured);

  // A repo, grown the way an agent grows one.
  const fs::path dir = fs::temp_directory_path() / "ge_cli_loop_lab";
  std::error_code ec;
  fs::remove_all(dir, ec);
  const fs::path repo = dir / "snow-racer";
  const Answer made = timed("ge new", [&] {
    const NewResult r = new_game({repo, "", ""});
    Outcome o;
    o.findings = r.findings;
    o.written = r.written;
    o.summary = "scaffolded";
    return o;
  });
  if (made.text.find(": error:") != std::string::npos) {
    lab.note("ge new could not scaffold here (no symlinks?), so the repo's commands were skipped");
    return;
  }
  record("new", "ge new", made, 50, Det::measured);
  record("add-system", "ge add system",
         timed("ge add system", [&] { return run_add(repo, "system", "wind", "", false); }), 8,
         Det::measured);
  record("add-lab", "ge add lab",
         timed("ge add lab", [&] { return run_add(repo, "lab", "wind", "scenario", false); }), 8,
         Det::measured);
  record("add-test", "ge add test",
         timed("ge add test", [&] { return run_add(repo, "test", "wind", "", false); }), 8,
         Det::measured);
  record("use", "ge use", timed("ge use", [&] { return run_use(repo, "racing"); }), 10,
         Det::measured);
  record("doctor", "ge doctor",
         timed("ge doctor",
               [&] {
                 Outcome o;
                 o.findings = doctor(repo);
                 o.summary = "ok";
                 return o;
               }),
         12, Det::measured);
  record("find", "ge find",
         timed("ge find", [&] { return run_find(repo, "", {"heightfield"}, 15); }), 40,
         Det::measured);
  record("explain", "ge explain",
         timed("ge explain", [&] { return run_explain(repo, "", "make_rng"); }), 30, Det::measured);
  fs::remove_all(dir, ec);
}

const ge::lab::LabSpec kSpec{
    "cli-loop",
    ge::lab::LabKind::bench,
    "engine",
    "engine/cli",
    "agent-interface",
    "How much does each ge command of an agent's loop print, and how long does it take?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace

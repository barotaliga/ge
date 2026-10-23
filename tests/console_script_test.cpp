// The console language run on fixtures: each fixture of tests/golden/console_script.txt is a cfg
// run in a fresh console, and what it printed is held line for line. The fixtures are the
// language's specification by example (docs/console.md): a change to what a cfg does rewrites
// the file under GE_GOLDEN_WRITE=1 (`make ge ARGS="golden write console_script_test"`) and the
// PR says why each moved line is right.
//
// The fixture file: `=== <name>` starts a fixture, its cfg lines follow, `---` ends the cfg and
// the printed lines follow until the next `===`.
//
// ge:summary Tests the console language on golden fixtures: if, set_calc, inc, cycle, triggers
// ge:keywords test console script fixture golden if set_calc inc dec toggle cycle msg_trigger
// ge:skill console testing
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/cfg.hpp"
#include "console/console.hpp"

using namespace ge::console;

namespace {

struct Fixture {
  std::string name, cfg;
  std::vector<std::string> want;
};

std::vector<Fixture> read_fixtures(const std::string& path) {
  std::vector<Fixture> out;
  std::ifstream in(path);
  std::string line;
  bool in_cfg = false;
  while (std::getline(in, line)) {
    if (line.rfind("=== ", 0) == 0) {
      out.push_back({line.substr(4), {}, {}});
      in_cfg = true;
    } else if (out.empty()) {
      continue;
    } else if (line == "---" && in_cfg) {
      in_cfg = false;
    } else if (in_cfg) {
      out.back().cfg += line + "\n";
    } else {
      out.back().want.push_back(line);
    }
  }
  for (Fixture& f : out)  // the blank line before the next fixture
    while (!f.want.empty() && f.want.back().empty()) f.want.pop_back();
  return out;
}

// A fixture's console: one game cvar and one game message, so `msg_trigger` has lines to match.
std::vector<std::string> run_fixture(const Fixture& f) {
  Console c;
  c.cvars().add({.name = "cl_fov", .fallback = "90", .flags = kArchive, .min = 10, .max = 140});
  c.add_command("game_say", "a line from the game",
                [](Console& con, const Args& a) { con.message(a.rest(0)); });
  // Files live in memory: a write prints the file, a read finds what was written.
  auto files = std::make_shared<std::map<std::string, std::string>>();
  c.set_files(
      [files](std::string_view name, std::string& text) {
        const auto it = files->find(std::string(name));
        if (it == files->end()) return false;
        text = it->second;
        return true;
      },
      [files, &c](std::string_view name, std::string_view text) {
        (*files)[std::string(name)] = std::string(text);
        c.print("[" + std::string(name) + "]");
        size_t at = 0;
        while (at < text.size()) {
          const size_t nl = text.find('\n', at);
          c.print("  " + std::string(text.substr(at, nl - at)));
          at = nl == std::string_view::npos ? text.size() : nl + 1;
        }
        return true;
      });
  c.execute(f.cfg);
  return c.output();
}

}  // namespace

TEST(fixtures) {
  const std::string path = ge::test::root_path("tests/golden/console_script.txt");
  std::vector<Fixture> fixtures = read_fixtures(path);
  CHECK_MSG(!fixtures.empty(), "no fixtures read from " + path + ": check the file is there");
  if (std::getenv("GE_GOLDEN_WRITE") != nullptr) {
    std::ostringstream out;
    for (Fixture& f : fixtures) {
      out << "=== " << f.name << "\n" << f.cfg << "---\n";
      for (const std::string& l : run_fixture(f)) out << l << "\n";
      out << "\n";
    }
    std::ofstream(path) << out.str();
    return;
  }
  for (const Fixture& f : fixtures) {
    const std::vector<std::string> got = run_fixture(f);
    CHECK_MSG(got == f.want, "fixture '" + f.name + "' printed " + std::to_string(got.size()) +
                                 " lines unlike its golden: rerun with GE_GOLDEN_WRITE=1 and "
                                 "read the diff of tests/golden/console_script.txt");
  }
}

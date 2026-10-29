// `ge use` and `ge add` (engine/cli/add.hpp) on a freshly scaffolded repo: a system, every lab
// kind and a test are written with every placeholder filled and pass `ge doctor`; a file that
// exists is never written over; a dry run writes nothing; `ge use` links the plugin once and
// syncs its skill and the skills of the plugins it brings; `ge add water` lays a lake, links
// ge::water once and syncs its skill. That the pieces build and pass is the packaging workflow's
// `scaffold` job.
//
// ge:summary Tests: ge add writes filled, doctor-clean pieces; ge use links a plugin and its skills
// ge:keywords test cli ge use add system lab test template plugin skills_for add-water lake
// ge:skill agent-interface testing
// ge:test tests/cli_add_test.cpp
#include <string>

#include "check.hpp"
#include "cli/add.hpp"
#include "cli/doctor.hpp"
#include "cli/scaffold.hpp"
#include "cli/skills.hpp"

using namespace ge::cli;

namespace {

std::string say(const std::vector<Finding>& findings) {
  std::string s;
  for (const Finding& f : findings) s += format_finding("ge", f);
  return s;
}

bool symlinks_work() {
  const fs::path dir = fs::temp_directory_path() / "ge_cli_add_test_probe";
  fs::remove_all(dir);
  fs::create_directories(dir);
  std::error_code ec;
  fs::create_symlink("target", dir / "link", ec);
  return !ec;
}

fs::path new_repo(const std::string& name) {
  const fs::path dir = fs::temp_directory_path() / ("ge_cli_add_test_" + name);
  fs::remove_all(dir);
  const fs::path repo = dir / "snow-racer";
  new_game({repo, "", ""});
  return repo;
}

}  // namespace

TEST(add_writes_every_piece_filled_and_doctor_clean) {
  if (!symlinks_work()) {
    ge::test::skip("this machine cannot create symlinks, which every game repo's guidance needs");
    return;
  }
  const fs::path repo = new_repo("pieces");
  struct Piece {
    const char* kind;
    const char* name;
    const char* lab_kind;
    const char* file;
  };
  const Piece pieces[] = {
      {"system", "wind-gust", "", "sim/wind_gust.hpp"},
      {"lab", "wind_sweep", "", "labs/wind_sweep_lab.cpp"},
      {"lab", "wind_hold", "scenario", "labs/wind_hold_lab.cpp"},
      {"lab", "wind_look", "picture", "labs/wind_look_lab.cpp"},
      {"lab", "wind_cost", "bench", "labs/wind_cost_lab.cpp"},
      {"test", "wind", "", "tests/wind_test.cpp"},
  };
  for (const Piece& p : pieces) {
    const Outcome o = run_add(repo, p.kind, p.name, p.lab_kind, false);
    CHECK_MSG(exit_of(o) == 0, say(o.findings));
    const std::string text = read_text(repo / p.file).value_or("");
    CHECK_MSG(!text.empty(), std::string(p.file) + " was not written");
    for (const char* placeholder :
         {"{{name}}", "{{ident}}", "{{snake}}", "{{kebab}}", "{{Type}}"}) {
      CHECK_MSG(text.find(placeholder) == std::string::npos,
                std::string(p.file) + " still holds " + placeholder);
    }
  }
  const std::string system = read_text(repo / "sim/wind_gust.hpp").value_or("");
  CHECK(system.find("struct WindGustState") != std::string::npos);
  CHECK(system.find("namespace snow_racer::sim") != std::string::npos);
  CHECK(read_text(repo / "tests/sim_wind_gust_test.cpp").has_value());
  CHECK(read_text(repo / "labs/wind_hold_lab.cpp").value_or("").find("\"wind-hold\"") !=
        std::string::npos);
  const auto findings = doctor(repo);
  CHECK_MSG(count_of(findings, Severity::error) == 0, say(findings));
}

TEST(add_never_writes_over_and_a_dry_run_writes_nothing) {
  if (!symlinks_work()) {
    ge::test::skip("this machine cannot create symlinks, which every game repo's guidance needs");
    return;
  }
  const fs::path repo = new_repo("refuse");
  const Outcome dry = run_add(repo, "system", "gust", "", true);
  CHECK(exit_of(dry) == 0 && dry.written.empty());
  CHECK(!fs::exists(repo / "sim/gust.hpp"));
  CHECK(exit_of(run_add(repo, "system", "gust", "", false)) == 0);
  const Outcome again = run_add(repo, "system", "gust", "", false);
  CHECK(exit_of(again) == 1 && again.written.empty());
  CHECK(exit_of(run_add(repo, "lab", "x", "nope", false)) == 2);
  CHECK(exit_of(run_add(repo, "test", "Bad Name", "", false)) == 2);
  CHECK(exit_of(run_add(repo, "test", "x", "sim", false)) == 2);  // --kind is for labs
}

TEST(use_links_a_plugin_once_and_syncs_its_skills) {
  if (!symlinks_work()) {
    ge::test::skip("this machine cannot create symlinks, which every game repo's guidance needs");
    return;
  }
  const fs::path repo = new_repo("use");
  CHECK(!links_plugin(repo, "skier"));
  CHECK(!fs::exists(repo / kSkillsDir / "skier"));
  const Outcome o = run_use(repo, "skier");
  CHECK_MSG(exit_of(o) == 0, say(o.findings));
  CHECK(links_plugin(repo, "skier"));
  CHECK(read_text(repo / "CMakeLists.txt").value_or("").find("PUBLIC ge::core ge::skier") !=
        std::string::npos);
  CHECK(fs::exists(repo / kSkillsDir / "skier" / "SKILL.md"));
  CHECK(fs::exists(repo / kSkillsDir / "mountain" / "SKILL.md"));  // skier brings mountain
  CHECK(!fs::exists(repo / kSkillsDir / "racing"));
  const Outcome twice = run_use(repo, "skier");
  CHECK(twice.written.empty() && twice.summary.find("already") != std::string::npos);
  CHECK(exit_of(run_use(repo, "nope")) == 2);
  const auto findings = doctor(repo);
  CHECK_MSG(count_of(findings, Severity::error) == 0, say(findings));
}

TEST(add_water_lays_a_lake_links_water_once_and_syncs_its_skill) {
  if (!symlinks_work()) {
    ge::test::skip("this machine cannot create symlinks, which every game repo's guidance needs");
    return;
  }
  const fs::path repo = new_repo("water");
  CHECK(!links_plugin(repo, "water"));
  CHECK(!fs::exists(repo / kSkillsDir / "water"));  // synced only once the repo links ge::water
  const Outcome o = run_add(repo, "water", "", "", false);
  CHECK_MSG(exit_of(o) == 0, say(o.findings));
  for (const char* file :
       {"sim/water.hpp", "sim/water.cpp", "labs/water_lab.cpp", "tests/sim_water_test.cpp"}) {
    const std::string text = read_text(repo / file).value_or("");
    CHECK_MSG(!text.empty(), std::string(file) + " was not written");
    for (const char* placeholder :
         {"{{name}}", "{{ident}}", "{{snake}}", "{{kebab}}", "{{Type}}"}) {
      CHECK_MSG(text.find(placeholder) == std::string::npos,
                std::string(file) + " still holds " + placeholder);
    }
  }
  CHECK(read_text(repo / "sim/water.hpp").value_or("").find("make_water(") != std::string::npos);
  CHECK(read_text(repo / "CMakeLists.txt").value_or("").find("PUBLIC ge::core ge::water") !=
        std::string::npos);
  CHECK(fs::exists(repo / kSkillsDir / "water" / "SKILL.md"));
  CHECK(exit_of(run_add(repo, "water", "", "", false)) == 1);  // never written over
  const Outcome harbour = run_add(repo, "water", "harbour", "", false);
  CHECK_MSG(exit_of(harbour) == 0, say(harbour.findings));
  CHECK(read_text(repo / "labs/harbour_lab.cpp").value_or("").find("\"harbour\"") !=
        std::string::npos);
  const std::string cmake = read_text(repo / "CMakeLists.txt").value_or("");
  CHECK(cmake.find("ge::water") == cmake.rfind("ge::water"));  // linked once
  const auto findings = doctor(repo);
  CHECK_MSG(count_of(findings, Severity::error) == 0, say(findings));
}

TEST(the_engine_s_plugins_are_carried_with_their_links) {
  bool skier = false;
  for (const PluginInfo& p : plugin_infos()) {
    if (p.name == "skier") {
      skier = true;
      CHECK(p.plugins.size() == 1 && p.plugins[0] == "mountain");
    }
  }
  CHECK(skier);
}

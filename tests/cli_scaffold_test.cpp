// `ge new` (engine/cli/scaffold.hpp): a game repo scaffolded from nothing has every scaffold file
// with its placeholders filled, the guidance's links, the engine's skills, and passes its own
// `ge doctor` before a line of the game is written; it refuses a bad name and a non-empty
// directory. Building the scaffold against this tree is the packaging workflow's `scaffold` job.
//
// ge:summary Tests: ge new writes a complete repo that passes ge doctor; refuses bad input
// ge:keywords test cli ge new scaffold placeholders symlinks doctor game-repo
// ge:skill agent-interface testing
// ge:test tests/cli_scaffold_test.cpp
#include <filesystem>
#include <string>

#include "check.hpp"
#include "cli/doctor.hpp"
#include "cli/embedded.hpp"
#include "cli/scaffold.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

namespace {

std::string say(const std::vector<Finding>& findings) {
  std::string s;
  for (const Finding& f : findings) s += format_finding("ge", f);
  return s;
}

fs::path scratch(const std::string& name) {
  const fs::path dir = fs::temp_directory_path() / ("ge_cli_scaffold_test_" + name);
  fs::remove_all(dir);
  return dir;
}

// Whether the machine lets this process make symlinks (Windows without Developer Mode does not).
bool symlinks_work() {
  const fs::path dir = scratch("probe");
  fs::create_directories(dir);
  std::error_code ec;
  fs::create_symlink("target", dir / "link", ec);
  return !ec;
}

}  // namespace

TEST(a_new_repo_is_complete_and_passes_its_own_doctor) {
  if (!symlinks_work()) {
    ge::test::skip("this machine cannot create symlinks, which every game repo's guidance needs");
    return;
  }
  const fs::path repo = scratch("snow-racer") / "snow-racer";
  const NewResult r = new_game({repo, "", ""});
  CHECK_MSG(r.findings.empty(), say(r.findings));

  int files = 0;
  for (const EmbeddedFile& f : embedded_files()) {
    if (!f.path.starts_with("scaffold/")) continue;
    std::string rel(f.path.substr(9));
    if (rel.ends_with(".in")) rel.resize(rel.size() - 3);
    const auto text = read_text(repo / rel);
    CHECK_MSG(text.has_value(), rel + " was not written; next: read new_game in scaffold.cpp");
    const std::string body = text.value_or("");
    for (const char* placeholder : {"{{name}}", "{{ident}}", "{{engine_"}) {
      CHECK_MSG(body.find(placeholder) == std::string::npos,
                rel + " still holds " + placeholder + "; next: fill it in new_game (scaffold.cpp)");
    }
    files++;
  }
  CHECK(files >= 10);
  CHECK(read_text(repo / "CMakeLists.txt").value_or("").find("project(snow-racer") !=
        std::string::npos);
  CHECK(read_text(repo / "sim/game.hpp").value_or("").find("namespace snow_racer::sim") !=
        std::string::npos);
  CHECK(read_text(repo / kPinFile)
            .value_or("")
            .find("set(GE_ENGINE_VERSION " + std::string(engine_version()) + ")") !=
        std::string::npos);
  for (const Link& link : kLinks) {
    CHECK_MSG(fs::is_symlink(repo / link.path), std::string(link.path) + " is not a symlink");
  }

  const auto findings = doctor(repo);
  CHECK_MSG(count_of(findings, Severity::error) == 0, say(findings));
  CHECK(count_of(findings, Severity::note) == 1);  // the pin carries no checksum
}

TEST(a_checksum_given_is_pinned_and_quiets_the_note) {
  if (!symlinks_work()) return;
  const fs::path repo = scratch("sum") / "sum";
  const std::string sum(64, 'a');
  const NewResult r = new_game({repo, "summed", sum});
  CHECK_MSG(r.findings.empty(), say(r.findings));
  const auto pin = read_pin(repo);
  CHECK(pin && pin->sha256 == sum);
  CHECK_MSG(doctor(repo).empty(), say(doctor(repo)));
}

TEST(new_refuses_a_bad_name_and_a_non_empty_directory) {
  CHECK(valid_game_name("snow-racer") && valid_game_name("g2"));
  CHECK(!valid_game_name("Snow") && !valid_game_name("2fast") && !valid_game_name("a--b") &&
        !valid_game_name("trail-"));
  CHECK(ident_of("snow-racer") == "snow_racer");

  const fs::path bad = scratch("bad");
  NewResult r = new_game({bad / "x", "Not A Name", ""});
  CHECK(r.findings.size() == 1 && r.findings[0].next.find("--name=") != std::string::npos);

  const fs::path full = scratch("full");
  std::string error;
  write_text(full / "README.md", "already a repo\n", error);
  r = new_game({full, "full", ""});
  CHECK(r.findings.size() == 1 && r.findings[0].what.find("not empty") != std::string::npos);
  CHECK(read_text(full / "README.md").value_or("") == "already a repo\n");
}

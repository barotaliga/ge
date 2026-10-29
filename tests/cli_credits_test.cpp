// engine/cli/credits.hpp, held on a content directory written for the test:
//
//   * THE PARSE: sections, their licences and their tables' quoted paths, each with its line.
//   * THE LICENCES: CC0, public domain, CC BY, MIT and the repository's own pass; share-alike,
//     noncommercial, no-derivatives and GPL fail; anything else is a warning.
//   * THE CHECK: an uncredited asset, a credit with no file and a missing licence each an error;
//     a clean directory none.
//
// ge:summary Tests for ge credits check: the parse, the licence verdicts, the directory check
// ge:keywords test cli credits licence cc0 check
// ge:skill agent-interface assets testing
#include <filesystem>
#include <fstream>
#include <string>

#include "check.hpp"
#include "cli/credits.hpp"

namespace c = ge::cli;
namespace fs = std::filesystem;

namespace {

void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p) << text;
}

int errors(const c::Outcome& o) { return c::count_of(o.findings, c::Severity::error); }

}  // namespace

TEST(credits_parse_into_sections) {
  const auto s = c::parse_credits(
      "# Credits\n\nintro\n\n## Kit A\n\n- Id: `kenney/a`\n- Licence: CC0 1.0 (link)\n\n"
      "| Our file | Source file |\n| --- | --- |\n| `sounds/a.wav` | `Audio/a.ogg` |\n"
      "| `models/b.glb` | `b.glb` |\n\n## Made here\n\n- License: the repository's own\n");
  CHECK(s.size() == 2);
  CHECK(s[0].title == "Kit A" && s[0].licence == "CC0 1.0 (link)" && s[0].line == 5);
  CHECK(s[0].files.size() == 2 && s[0].files[1] == "models/b.glb" && s[0].file_lines[0] == 12);
  CHECK(s[1].licence == "the repository's own" && s[1].files.empty());
}

TEST(licences_this_repository_can_carry) {
  using S = c::Severity;
  CHECK(c::licence_verdict("CC0 1.0") == S::note);
  CHECK(c::licence_verdict("Public domain") == S::note);
  CHECK(c::licence_verdict("CC-BY 4.0") == S::note);
  CHECK(c::licence_verdict("MIT") == S::note);
  CHECK(c::licence_verdict("CC-BY-SA 3.0") == S::error);
  CHECK(c::licence_verdict("CC BY-NC 4.0") == S::error);
  CHECK(c::licence_verdict("CC BY-ND 4.0") == S::error);
  CHECK(c::licence_verdict("GPL 3.0") == S::error);
  CHECK(c::licence_verdict("") == S::error);
  CHECK(c::licence_verdict("Royalty-free bundle") == S::warn);
}

TEST(a_content_directory_is_held_to_its_credits) {
  const fs::path dir = fs::temp_directory_path() / "ge-cli-credits-test";
  fs::remove_all(dir);
  write(dir / "sounds/door/stop.wav", "x");
  write(dir / "models/crate.glb", "x");
  write(dir / "README.md", "not an asset");
  write(dir / "CREDITS.md",
        "## Kit\n- Licence: CC0 1.0\n| Our file | Source |\n| --- | --- |\n"
        "| `sounds/door/stop.wav` | `a.ogg` |\n| `models/crate.glb` | `crate.glb` |\n");
  CHECK(c::asset_files(dir).size() == 2);
  CHECK_MSG(
      errors(c::run_credits_check(dir)) == 0,
      c::run_credits_check(dir).findings.empty() ? "" : c::run_credits_check(dir).findings[0].what);

  write(dir / "sounds/new.wav", "x");    // uncredited
  fs::remove(dir / "models/crate.glb");  // a credit with no file
  const c::Outcome o = c::run_credits_check(dir);
  CHECK(errors(o) == 2);
  bool uncredited = false, gone = false;
  for (const c::Finding& f : o.findings) {
    uncredited = uncredited || f.what == "sounds/new.wav has no credit";
    gone = gone || f.what.find("models/crate.glb, which is not there") != std::string::npos;
  }
  CHECK(uncredited && gone);

  write(dir / "CREDITS.md",
        "## Kit\n| `sounds/door/stop.wav` | `a.ogg` |\n| `sounds/new.wav` | `b` |\n");
  CHECK(errors(c::run_credits_check(dir)) == 1);  // no licence line
  fs::remove_all(dir);
}

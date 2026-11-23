// `ge manual`: two programs' manual sources merged into one page and its JSON, a missing or damaged
// source a finding.
//
// ge:summary Tests for ge manual: sources merged into the page and its JSON, bad sources refused
// ge:keywords cli manual run_manual user-manual test
// ge:skill agent-interface testing
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "cli/manual.hpp"

namespace c = ge::cli;
namespace fs = std::filesystem;

namespace {

void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p) << text;
}

std::string read(const fs::path& p) {
  std::ifstream f(p);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}

int errors(const c::Outcome& o) { return c::count_of(o.findings, c::Severity::error); }

const fs::path kDir = fs::temp_directory_path() / "ge_cli_manual_test";

}  // namespace

TEST(two_programs_sources_make_one_page) {
  fs::remove_all(kDir);
  write(kDir / "se.txt",
        "# manual 1\n# program se Shooter's Edge\n# game q1 Quake\n"
        "setting\tse\tq1\tsv_hostname\tengine\ttext\tunnamed\t\t\tA.......\t\t\tthe name\t\n"
        "command\tse\tq1\tconnect\tengine\t\t\t\t\t\t\t\tconnect <server>: join it\t\n");
  write(kDir / "sesv.txt",
        "# manual 1\n# program sesv Dedicated server\n# game qw QuakeWorld | q1\n"
        "setting\tsesv\tqw\tsv_hostname\tengine\ttext\tunnamed\t\t\tA.......\t\t\tthe name\t\n"
        "flag\tsesv\tqw\t--port=\t\t\t\t\t\t\tN\t\tThe UDP port.\t\n");
  c::ManualOptions o;
  o.sources = {(kDir / "se.txt").string(), (kDir / "sesv.txt").string()};
  o.out = (kDir / "site" / "user-manual.html").string();
  o.title = "Shooter's Edge manual";
  const c::Outcome out = c::run_manual(o);
  CHECK(errors(out) == 0);
  CHECK(out.summary.find("1 settings, 1 commands and 1 flags") != std::string::npos);
  const std::string html = read(kDir / "site" / "user-manual.html");
  CHECK(html.find("\"n\":\"sv_hostname\",\"p\":[\"se\",\"sesv\"]") != std::string::npos);
  CHECK(html.find("\"takes_in\":[\"q1\"]") != std::string::npos);
  CHECK(read(kDir / "site" / "manual.json").find("\"n\":\"--port=\"") != std::string::npos);
  fs::remove_all(kDir);
}

TEST(a_missing_or_damaged_source_is_a_finding) {
  fs::remove_all(kDir);
  write(kDir / "bad.txt", "# manual 1\nsetting\tse\n");
  c::ManualOptions o;
  o.sources = {(kDir / "none.txt").string(), (kDir / "bad.txt").string()};
  o.out = (kDir / "user-manual.html").string();
  const c::Outcome out = c::run_manual(o);
  CHECK(errors(out) == 2);
  CHECK(!fs::exists(kDir / "user-manual.html"));
  fs::remove_all(kDir);
}

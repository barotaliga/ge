// `ge pak` (engine/cli/pak.hpp): a directory packed, listed, converted pack to zip and back, its
// files summed and held to the sums, one named file extracted byte for byte, and an entry that
// would leave --out refused.
//
// ge:summary Test: ge pak build, ls, convert, sum --check and extract round trips
// ge:keywords test cli pak pk3 build ls convert sum check extract refuse
// ge:skill testing agent-interface vfs
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.hpp"
#include "cli/pak.hpp"
#include "vfs/pak.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

namespace {

Outcome pak(const std::vector<std::string>& words) {
  std::vector<const char*> argv = {"ge", "pak"};
  for (const std::string& w : words) argv.push_back(w.c_str());
  const Args a = parse_args(static_cast<int>(argv.size()), argv.data());
  return run_pak(a, std::string(a.command->name).substr(4));
}

void put(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

std::string slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

fs::path fresh(const char* name) {
  const fs::path dir = fs::temp_directory_path() / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

}  // namespace

TEST(pak_build_ls_convert_sum_and_extract_round_trip) {
  const fs::path dir = fresh("ge_cli_pak_test");
  put(dir / "src/gfx/palette.lmp", std::string(768, '\x07'));
  put(dir / "src/maps/start.bsp", "a map");
  const std::string p = (dir / "a.pak").string(), z = (dir / "a.pk3").string(),
                    back = (dir / "b.pak").string(), sums = (dir / "a.sums").string();
  CHECK(pak({"build", (dir / "src").string(), "--out=" + p}).findings.empty());
  const Outcome ls = pak({"ls", p, "--sha256"});
  CHECK(ls.findings.empty());
  CHECK(ls.text.find("gfx/palette.lmp") != std::string::npos);
  CHECK(ls.result.find("\"kind\":\"pak\"") != std::string::npos);
  CHECK(pak({"convert", p, z}).findings.empty());
  CHECK(pak({"ls", z}).result.find("\"kind\":\"zip\"") != std::string::npos);
  CHECK(pak({"convert", z, back}).findings.empty());
  CHECK(slurp(p) == slurp(back));  // pack -> zip -> pack keeps every byte and the order
  const Outcome sum = pak({"sum", p, "--out=" + sums});
  CHECK(sum.findings.empty() && sum.summary.find("2 file(s)") != std::string::npos);
  const Outcome held = pak({"sum", z, "--check=" + sums});
  CHECK(held.findings.empty());
  CHECK(held.result.find("\"ok\":2") != std::string::npos);
  const Outcome one = pak({"sum", z, "MAPS/START.BSP"});  // a zip's names fold case
  CHECK(one.findings.empty() && one.text.find("maps/start.bsp") != std::string::npos);
  const Outcome ex = pak({"extract", z, "maps/start.bsp", "--out=" + (dir / "out").string()});
  CHECK(ex.findings.empty() && ex.written.size() == 1);
  CHECK(slurp(dir / "out/maps/start.bsp") == "a map");
  CHECK(!fs::exists(dir / "out/gfx/palette.lmp"));
}

TEST(pak_sum_check_names_a_changed_and_a_missing_file) {
  const fs::path dir = fresh("ge_cli_pak_check_test");
  put(dir / "a/x.txt", "one");
  put(dir / "a/y.txt", "two");
  const std::string p = (dir / "a.pak").string(), sums = (dir / "a.sums").string();
  CHECK(pak({"build", (dir / "a").string(), "--out=" + p}).findings.empty());
  CHECK(pak({"sum", p, "--out=" + sums}).findings.empty());
  fs::remove(dir / "a/y.txt");
  put(dir / "a/x.txt", "ONE");
  CHECK(pak({"build", (dir / "a").string(), "--out=" + p}).findings.empty());
  const Outcome o = pak({"sum", p, "--check=" + sums});
  CHECK(o.findings.size() == 2);
  CHECK(o.result.find("\"changed\":1") != std::string::npos);
  CHECK(o.result.find("\"missing\":1") != std::string::npos);
  CHECK(exit_of(o) == 1);
}

TEST(pak_extract_refuses_a_path_that_leaves_out) {
  const fs::path dir = fresh("ge_cli_pak_evil_test");
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(ge::vfs::write_pak({{"../evil.txt", {'x'}}, {"ok.txt", {'y'}}}, &bytes, &error));
  std::ofstream(dir / "e.pak", std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  const Outcome o = pak({"extract", (dir / "e.pak").string(), "--out=" + (dir / "out").string()});
  CHECK(o.findings.size() == 1 && o.written.size() == 1);
  CHECK(!fs::exists(dir / "evil.txt"));
  CHECK(safe_entry_path("gfx/a.lmp") && !safe_entry_path("/etc/x") && !safe_entry_path("a/../b"));
  CHECK(!safe_entry_path("c:x") && !safe_entry_path("dir/"));
  std::string hex, path;
  CHECK(parse_sum_line(std::string(64, 'A') + "  a/b.txt", &hex, &path));
  CHECK(hex == std::string(64, 'a') && path == "a/b.txt");
  CHECK(!parse_sum_line("zz  a", &hex, &path));
}

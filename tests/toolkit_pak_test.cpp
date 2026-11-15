// The toolkit's pak group (engine/toolkit/pak.hpp), as `ge pak` and every game's player CLI run it:
// a directory packed, listed, converted pack to zip and back, its files summed and held to the
// sums, one named file extracted byte for byte, an entry that would leave --out refused, and the
// next steps spelled with the calling program's name.
//
// ge:summary Test: the pak group's build, ls, convert, sum --check and extract round trips
// ge:keywords test cli toolkit gamecli pak pk3 build ls convert sum check extract refuse
// ge:skill testing agent-interface vfs
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.hpp"
#include "toolkit/call.hpp"
#include "toolkit/pak.hpp"
#include "vfs/pak.hpp"

namespace fs = std::filesystem;
using namespace ge::toolkit;

namespace {

// `pak <sub> <words...> [--flag[=value]...]` called as `program` would call it.
Outcome pak(const std::vector<std::string>& words, const std::string& program = "ge") {
  Call call;
  call.program = program;
  for (size_t i = 1; i < words.size(); i++) {
    const std::string& w = words[i];
    if (w.starts_with("--")) {
      const size_t eq = w.find('=');
      call.flags.emplace_back(w.substr(2, eq == std::string::npos ? std::string::npos : eq - 2),
                              eq == std::string::npos ? "" : w.substr(eq + 1));
    } else {
      call.words.push_back(w);
    }
  }
  const Row* row = find_row("pak " + words[0]);
  CHECK_MSG(row != nullptr, "no toolkit row 'pak " + words[0] + "'");
  return row != nullptr ? run_row(*row, call) : Outcome{};
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

TEST(pak_ls_over_a_game_directory_names_each_winner_and_what_it_hides) {
  const fs::path root = fresh("ge_cli_pak_game_test");
  put(root / "src/a.txt", "base a");
  put(root / "src/b.txt", "pak b");
  CHECK(pak({"build", (root / "src").string(), "--out=" + (root / "base/pak0.pak").string()})
            .findings.empty());
  fs::remove_all(root / "src");
  put(root / "src/B.TXT", "zip b");
  CHECK(pak({"build", (root / "src").string(), "--out=" + (root / "base/z.pk3").string()})
            .findings.empty());
  put(root / "base/b.txt", "loose b");
  put(root / "mod/a.txt", "mod a");
  const Outcome o = pak({"ls", (root / "base").string(), "--mods=mod", "--sha256"});
  CHECK(o.findings.empty());
  CHECK(o.result.find("\"overridden\":2") != std::string::npos);
  // a.txt: the mod's loose file over the base pack; b.txt: the loose file over the zip and pack.
  CHECK(o.result.find("{\"path\":\"a.txt\",\"bytes\":5,\"from\":\"mod/\",\"hides\":"
                      "[\"base/pak0.pak\"]") != std::string::npos);
  CHECK(o.result.find("\"from\":\"base/\",\"hides\":[\"base/pak0.pak\",\"base/z.pk3\"]") !=
        std::string::npos);
  const Outcome hidden = pak({"ls", (root / "base").string(), "--mods=mod", "--hidden"});
  CHECK(hidden.result.find("\"path\":\"c.txt\"") == std::string::npos);
  CHECK(!pak({"ls", (root / "base/pak0.pak").string(), "--hidden"}).findings.empty());
}

TEST(a_players_cli_spells_the_next_step_with_its_own_name) {
  const fs::path dir = fresh("toolkit_pak_program");
  put(dir / "src/a.txt", "a");
  const std::string p = (dir / "a.pak").string();
  CHECK(pak({"build", (dir / "src").string(), "--out=" + p}, "secli").findings.empty());
  const Outcome ls = pak({"ls", p}, "secli");
  CHECK_MSG(ls.next.rfind("secli pak extract ", 0) == 0, "next was: " + ls.next);
}

TEST(a_wrong_word_count_is_a_usage_error) {
  const Outcome o = pak({"convert", "only-one.pak"});
  CHECK(o.exit == 2);
  CHECK(!o.findings.empty() && o.findings[0].next == "ge help pak convert");
}

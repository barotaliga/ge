// `secli mod run` (games/shooters_edge/cli/mod_run.hpp) on the sample package this tree builds: it
// plays the mod scenario twice with the package, answers that the replay holds and that the mod
// changed the match, and counts the hooks; a folder that is no package is refused with the
// loader's reason and `ge mod check` as the next step; a bad flag is refused as a usage error.
// With --wasm it plays the package's wasm32 module, to the native library's hash.
//
// ge:summary Tests secli mod run: the sample replays and changes the match; refusals
// ge:keywords test secli mod-run replay hook-calls refused package check-run
// ge:skill mod agent-interface testing
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "gamecli/gamecli.hpp"
#include "shooters_edge/cli/spec.hpp"

namespace fs = std::filesystem;
namespace gc = ge::gamecli;

namespace {

int secli(const std::vector<std::string>& args, std::string& out) {
  out.clear();
  return gc::run(shooters_edge::cli::spec(), args, out, fs::temp_directory_path().string());
}

}  // namespace

TEST(the_sample_replays_and_changes_the_match) {
  const std::string pkg = (fs::path(GE_MOD_DIR) / "rocket-bounce").string();
  std::string out;
  const int code = secli({"mod", "run", pkg, "--frames=400", "--json"}, out);
  CHECK_MSG(code == 0, out);
  CHECK_MSG(out.find("\"ok\":true") != std::string::npos, out);
  CHECK_MSG(out.find("\"replay\":\"holds\"") != std::string::npos, out);
  CHECK_MSG(out.find("\"changed\":true") != std::string::npos, out);
  CHECK_MSG(out.find("\"hook_calls\":0,") == std::string::npos, out);
  CHECK_MSG(out.find("\"frames\":400") != std::string::npos, out);
}

TEST(the_module_plays_as_a_store_build_would) {
  const fs::path pkg = fs::path(GE_MOD_DIR) / "rocket-bounce";
  if (!fs::exists(pkg / "rocket_bounce.wasm")) {
    std::printf("  skipped: no wasm32 module in the sample package (no clang with wasm-ld)\n");
    return;
  }
  std::string native, module;
  CHECK(secli({"mod", "run", pkg.string(), "--frames=400", "--json"}, native) == 0);
  const int code = secli({"mod", "run", pkg.string(), "--frames=400", "--wasm", "--json"}, module);
  CHECK_MSG(code == 0, module);
  CHECK_MSG(module.find("\"platform\":\"wasm32\"") != std::string::npos, module);
  CHECK_MSG(module.find("\"replay\":\"holds\"") != std::string::npos, module);
  // The same match: the modded hash is the native library's.
  const auto hash = [](const std::string& s) {
    const size_t at = s.find("\"modded_hash\":\"");
    return at == std::string::npos ? std::string() : s.substr(at + 15, 16);
  };
  CHECK_MSG(!hash(native).empty() && hash(native) == hash(module), native + "\n" + module);
}

TEST(a_folder_that_is_no_package_is_refused) {
  const fs::path dir = fs::temp_directory_path() / "ge_se_mod_run_empty";
  fs::remove_all(dir);
  fs::create_directories(dir);
  std::string out;
  CHECK(secli({"mod", "run", dir.string(), "--json"}, out) == 1);
  CHECK_MSG(out.find("the game refused") != std::string::npos, out);
  CHECK_MSG(out.find("ge mod check") != std::string::npos, out);
}

TEST(bad_words_are_usage_errors) {
  std::string out;
  CHECK(secli({"mod", "run", "--json"}, out) == 2);
  const std::string pkg = (fs::path(GE_MOD_DIR) / "rocket-bounce").string();
  CHECK(secli({"mod", "run", pkg, "--frames=none", "--json"}, out) == 2);
  CHECK_MSG(out.find("--frames") != std::string::npos, out);
}

// `ge mod` END TO END (engine/cli/mod.hpp): `ge mod new` writes a Shooter's Edge mod from the
// template with every placeholder filled; `ge mod build` builds it against this tree with CMake
// into a package the engine's loader accepts; `ge mod check` passes it and refuses the fixture
// built against mod ABI 2.0, naming the version; `ge mod pack` copies a package into one directory
// with a merged manifest and refuses to merge two different mods. An unknown game is a usage error.
//
// ge:summary Tests: ge mod new, build, check and pack on a scaffolded mod and the tree's packages
// ge:keywords test cli ge mod new build check pack template package loader
// ge:skill agent-interface mod testing
#include <cstdio>
#include <filesystem>
#include <string>

#include "check.hpp"
#include "cli/mod.hpp"
#include "mod/manifest.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

namespace {

fs::path scratch(const std::string& tag) {
  const fs::path p = fs::temp_directory_path() / ("ge_cli_mod_test_" + tag);
  fs::remove_all(p);
  return p;
}

bool any_error(const Outcome& o, const std::string& what) {
  for (const Finding& f : o.findings)
    if (f.severity == Severity::error && (f.what + " " + f.next).find(what) != std::string::npos)
      return true;
  return false;
}

bool clean(const Outcome& o) {
  for (const Finding& f : o.findings) {
    if (f.severity == Severity::error) {
      std::printf("  error: %s (%s) next: %s\n", f.what.c_str(), f.where.c_str(), f.next.c_str());
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(an_unknown_game_is_a_usage_error) {
  ModOptions o;
  o.dir = scratch("nogame");
  o.game = "pinball";
  const Outcome out = run_mod_new(o);
  CHECK(out.exit == 2);
  CHECK(any_error(out, "shooters_edge"));
}

TEST(check_passes_the_sample_and_refuses_another_abi) {
  ModOptions o;
  o.dir = fs::path(GE_MOD_DIR) / "rocket-bounce";
  const Outcome ok = run_mod_check(o);
  CHECK(clean(ok));
  CHECK(ok.result.find("\"state_bytes\"") != std::string::npos);
  o.dir = fs::path(GE_MOD_DIR) / "abi-two";
  const Outcome refused = run_mod_check(o);
  CHECK(any_error(refused, "mod ABI 2.0"));
}

TEST(pack_merges_one_mod_and_refuses_two) {
  ModOptions o;
  o.packages = {fs::path(GE_MOD_DIR) / "rocket-bounce"};
  o.out = scratch("pack").generic_string();
  const Outcome packed = run_mod_pack(o);
  CHECK(clean(packed));
  CHECK(fs::exists(fs::path(o.out) / "mod.manifest"));
  ModOptions again;
  again.dir = o.out;
  CHECK(clean(run_mod_check(again)));
  o.packages.push_back(fs::path(GE_MOD_DIR) / "abi-two");
  o.out = scratch("pack2").generic_string();
  CHECK(any_error(run_mod_pack(o), "not the rocket-bounce"));
}

TEST(new_then_build_makes_a_package_the_loader_accepts) {
  ModOptions o;
  o.dir = scratch("new") / "faster-rockets";
  o.game = "shooters_edge";
  o.engine = GE_SOURCE_DIR;
  const Outcome made = run_mod_new(o);
  CHECK(clean(made));
  CHECK(made.written.size() == 4);
  for (const std::string& rel : made.written) {
    const std::string text = read_text(o.dir / rel).value_or("");
    for (const char* p :
         {"{{name}}", "{{ident}}", "{{engine}}", "{{game}}", "{{game_include}}", "{{game_header}}"})
      CHECK_MSG(text.find(p) == std::string::npos, rel + " still holds " + p);
  }
  CHECK(any_error(run_mod_new(o), "not empty"));
  const Outcome built = run_mod_build(o);
  CHECK(clean(built));
  const fs::path package = o.dir / "build" / "mods" / "faster-rockets";
  CHECK(fs::exists(package / "mod.manifest"));
  CHECK(built.result.find("\"name\":\"faster-rockets\"") != std::string::npos ||
        built.result.find("\"name\": \"faster-rockets\"") != std::string::npos);
  ModOptions check;
  check.dir = package;
  check.game = "shooters_edge";
  CHECK(clean(run_mod_check(check)));
}

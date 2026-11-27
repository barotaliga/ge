// Tests: the mod group's bodies under either program's name: `mod check` passes the sample package
// and refuses the one built against mod ABI 2.0, its next step spelled with the caller's name;
// `mod pack` merges one mod and refuses two; `mod key` then `mod sign` sign a package, and a check
// with that key trusted names the signer. Shooter's Edge's CLI lists and runs the rows (secli mod
// check); ge's own wrappers and `--run` are held by tests/cli_mod_test.cpp.
//
// ge:summary Tests: the mod group checked, packed, keyed and signed, and run by secli
// ge:keywords test toolkit mod check pack key sign signature trusted-keys secli
// ge:skill agent-interface mod testing
// ge:test tests/toolkit_mod_test.cpp
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/json_read.hpp"
#include "gamecli/gamecli.hpp"
#include "shooters_edge/cli/spec.hpp"
#include "toolkit/mod.hpp"

namespace fs = std::filesystem;
using namespace ge::toolkit;

namespace {

fs::path scratch(const std::string& tag) {
  const fs::path p = fs::temp_directory_path() / ("ge_toolkit_mod_test_" + tag);
  fs::remove_all(p);
  return p;
}

bool any_error(const Outcome& o, const std::string& what) {
  for (const Finding& f : o.findings)
    if (f.severity == Severity::error && (f.what + " " + f.next).find(what) != std::string::npos)
      return true;
  return false;
}

bool clean(const Outcome& o) { return count_of(o.findings, Severity::error) == 0; }

std::string field(const std::string& json, const std::string& key) {
  ge::core::FlatJson j;
  std::string error;
  if (!ge::core::read_flat(json, j, error, true)) return "";
  return j[key].text;
}

}  // namespace

TEST(check_passes_the_sample_and_refuses_another_abi_in_the_callers_name) {
  ModArgs a;
  a.dir = fs::path(GE_MOD_DIR) / "rocket-bounce";
  const Outcome ok = mod_check("secli", a);
  CHECK(clean(ok));
  CHECK(field(ok.result, "game") == "shooters_edge");
  CHECK(ok.next.rfind("secli mod pack ", 0) == 0);
  a.dir = fs::path(GE_MOD_DIR) / "abi-two";
  CHECK(any_error(mod_check("secli", a), "mod ABI 2.0"));
  a.game = "pinball";
  const Outcome nogame = mod_check("secli", a);
  CHECK(nogame.exit == 2 && any_error(nogame, "shooters_edge"));
}

TEST(pack_merges_one_mod_and_refuses_two) {
  ModArgs a;
  a.packages = {fs::path(GE_MOD_DIR) / "rocket-bounce"};
  a.out = scratch("pack").generic_string();
  const Outcome packed = mod_pack("secli", a);
  CHECK(clean(packed));
  CHECK(fs::exists(fs::path(a.out) / "mod.manifest"));
  CHECK(packed.next.rfind("secli mod check ", 0) == 0);
  a.packages.push_back(fs::path(GE_MOD_DIR) / "abi-two");
  a.out = scratch("pack2").generic_string();
  CHECK(any_error(mod_pack("secli", a), "not the rocket-bounce"));
}

TEST(key_then_sign_then_a_trusted_check_names_the_signer) {
  const Outcome key = mod_key("secli");
  const std::string secret = field(key.result, "secret"), pub = field(key.result, "public");
  CHECK(secret.size() == 64 && pub.size() == 64);
  CHECK(key.next.find("secli mod sign") != std::string::npos);
  ModArgs a;
  a.packages = {fs::path(GE_MOD_DIR) / "rocket-bounce"};
  a.out = scratch("signed").generic_string();
  CHECK(clean(mod_pack("secli", a)));
  a.dir = a.out;
  a.key_env = "GE_TOOLKIT_MOD_TEST_KEY";
  CHECK(any_error(mod_sign("secli", a), "secli mod key"));  // no key in the environment yet
#ifdef _WIN32
  _putenv_s(a.key_env.c_str(), secret.c_str());
#else
  setenv(a.key_env.c_str(), secret.c_str(), 1);
#endif
  const Outcome signed_ = mod_sign("secli", a);
  CHECK(clean(signed_));
  CHECK(fs::exists(fs::path(a.out) / "mod.sig"));
  const fs::path trusted = fs::path(a.out).parent_path() / "ge_toolkit_mod_test_trusted.txt";
  std::ofstream(trusted) << pub << " the author\n";
  a.trusted = trusted.generic_string();
  const Outcome checked = mod_check("secli", a);
  CHECK(clean(checked));
  CHECK(field(checked.result, "signed") == "trusted");
  CHECK(field(checked.result, "signer") == "the author");
  fs::remove(trusted);
}

TEST(the_players_cli_runs_the_mod_group) {
  const fs::path root = scratch("secli");
  // Every call names its own install, state and logs, never the machine's.
  const auto secli = [&](std::vector<std::string> args, std::string& out) {
    args.insert(args.begin(),
                {"--fs_basegame=q1", "--fs_basedir_q1=" + root.string(),
                 "--state=" + (root / "state").string(), "--logs=" + (root / "logs").string()});
    return ge::gamecli::run(shooters_edge::cli::spec(), args, out, (root / "bin").string());
  };
  std::string out;
  const std::string pkg = (fs::path(GE_MOD_DIR) / "rocket-bounce").string();
  CHECK_MSG(secli({"mod", "check", pkg, "--json"}, out) == 0, out);
  CHECK(field(out, "command") == "secli mod check");
  CHECK(field(out, "next").rfind("secli mod pack ", 0) == 0);
  CHECK(secli({"help", "mod sign"}, out) == 0);
  CHECK(out.find("--key-env") != std::string::npos);
  fs::remove_all(root);
}

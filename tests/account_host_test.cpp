// Tests: the account service's files (engine/account/host.hpp): the key made once and read back
// the same, a damaged key refused, the store's log read back after changes were appended, a last
// line cut short dropped with a warning, and the service opened on a loopback socket.
//
// ge:summary Tests: the account key file and the store's log on disk, the hosted service
// ge:keywords test account host key-file accounts-log cut-short reload
// ge:skill net testing
#include "account/host.hpp"

#include <filesystem>
#include <fstream>

#include "account/store_text.hpp"
#include "check.hpp"

namespace ga = ge::account;
namespace gn = ge::net;
namespace fs = std::filesystem;

namespace {

constexpr int64_t kT0 = 1794500000ll * 1000000;

std::string fresh_dir(const char* name) {
  const fs::path p = fs::temp_directory_path() / name;
  fs::remove_all(p);
  fs::create_directories(p);
  return p.string();
}

ga::Fresh fresh(uint8_t b) {
  ga::Fresh f{};
  for (size_t i = 0; i < f.size(); i++) f[i] = static_cast<uint8_t>(b + i * 3);
  return f;
}

}  // namespace

TEST(account_key_is_made_once_and_read_back) {
  const std::string dir = fresh_dir("ge_account_key_test");
  const std::string path = dir + "/account.key";
  gn::Key a{}, b{};
  bool made = false;
  std::string error;
  CHECK(ga::load_or_make_key(path, a, made, error) && made);
  CHECK(ga::load_or_make_key(path, b, made, error) && !made && a == b);
  std::ofstream(path) << "not a key\n";
  CHECK(!ga::load_or_make_key(path, b, made, error));
  CHECK(error.find("64 hex digits") != std::string::npos);
  fs::remove_all(dir);
}

TEST(account_store_file_reads_back_and_drops_a_cut_line) {
  const std::string dir = fresh_dir("ge_account_store_test");
  const std::string path = dir + "/accounts.log";
  ga::Store s;
  std::string warning, error;
  CHECK(ga::load_store(path, s, ga::HashCost{8, 1}, kT0, warning, error));
  CHECK(s.accounts.empty() && warning.empty());
  CHECK(ga::create(s, "Niclas", "correct horse", kT0, fresh(1)).refusal == ga::Refusal::none);
  CHECK(ga::create(s, "Rocket", "long enough", kT0, fresh(2)).refusal == ga::Refusal::none);
  {
    std::ofstream f(path, std::ios::app | std::ios::binary);
    f << s.log << "account 3 Hal";  // the process stopped mid-line
  }
  ga::Store back;
  CHECK(ga::load_store(path, back, ga::HashCost{8, 1}, kT0, warning, error));
  CHECK(back.accounts.size() == 2 && ga::find_by_name(back, "rocket"));
  CHECK(warning.find("cut short") != std::string::npos);
  CHECK(ga::sign_in(back, "Niclas", "correct horse", "a", kT0).refusal == ga::Refusal::none);
  // The rewrite is the snapshot: reading it again warns of nothing.
  ga::Store again;
  warning.clear();
  CHECK(ga::load_store(path, again, ga::HashCost{8, 1}, kT0, warning, error));
  CHECK(again.accounts.size() == 2 && warning.empty());
  fs::remove_all(dir);
}

TEST(account_host_opens_on_loopback) {
  const std::string dir = fresh_dir("ge_account_host_test");
  gn::Address bind;
  CHECK(gn::parse_address("127.0.0.1:0", bind));
  ga::Host h;
  std::string error;
  CHECK(ga::host_open(h, dir, bind, ga::HashCost{8, 1}, kT0, error));
  CHECK(fs::exists(dir + "/account.key") && fs::exists(dir + "/accounts.log"));
  ga::host_update(h, kT0);
  ga::host_close(h);
  fs::remove_all(dir);
}

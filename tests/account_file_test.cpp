// Tests: a player's account file (engine/account/account_file.hpp): written and read back, readable
// by its owner alone, hex both ways, a missing file and bad hex refused.
//
// ge:summary Tests: the player's account file round trip, its permissions and hex
// ge:keywords test account-file account-txt hex
// ge:skill net testing
#include "account/account_file.hpp"

#include <filesystem>

#include "check.hpp"

namespace fs = std::filesystem;
namespace ga = ge::account;

TEST(account_file_round_trip_owner_only) {
  const fs::path dir = fs::temp_directory_path() / "ge_account_file_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  const std::string path = (dir / "cli" / "account.txt").string();
  ga::AccountFile f;
  CHECK(!ga::account_file_read(path, f));
  f.master = "play.example.net:27901";
  f.key = std::string(64, 'a');
  f.name = "Niclas";
  f.ticket = ga::bytes_to_hex({0x00, 0x7f, 0xff});
  CHECK(f.ticket == "007fff");
  CHECK(ga::account_file_write(path, f));
  ga::AccountFile back;
  CHECK(ga::account_file_read(path, back));
  CHECK(back.master == f.master && back.key == f.key && back.name == f.name &&
        back.ticket == f.ticket);
#ifndef _WIN32
  // POSIX modes: owner read and write only. Windows has no group or other bits (std::filesystem
  // reports a file's read-only flag for all three), and the file is private by the profile
  // folder's access list instead, so there is nothing for this check to read.
  const fs::perms p = fs::status(path).permissions();
  CHECK((p & (fs::perms::group_all | fs::perms::others_all)) == fs::perms::none);
#endif
  CHECK(ga::hex_to_bytes("007fff") == std::vector<uint8_t>({0x00, 0x7f, 0xff}));
  CHECK(ga::hex_to_bytes("0g").empty() && ga::hex_to_bytes("abc").empty());
  fs::remove_all(dir, ec);
}

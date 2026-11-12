// Tests: a server's ban list (engine/server/bans.hpp): patterns with missing parts, `*` and bits;
// ban and safe entries under both modes; times; the console words and their answers.
//
// ge:summary Tests: ban patterns, the two modes, safe entries, times, addip removeip listip writeip
// ge:keywords test ban addip removeip listip writeip filterban sv_banmode
// ge:skill net testing
#include <string>

#include "check.hpp"
#include "server/bans.hpp"

namespace gs = ge::server;
namespace gn = ge::net;

TEST(patterns_match_a_subnet_by_parts_stars_or_bits) {
  gs::BanEntry e;
  CHECK(gs::parse_ban_pattern("10.0", e) && e.ip == 0x0A000000u && e.mask == 0xFFFF0000u);
  CHECK(gs::parse_ban_pattern("10.0.*.7", e) && e.mask == 0xFFFF00FFu);
  CHECK(gs::parse_ban_pattern("10.0.4.0/22", e) && e.mask == 0xFFFFFC00u);
  CHECK(gs::parse_ban_pattern("10.0.", e) && e.mask == 0xFFFF0000u);
  CHECK(!gs::parse_ban_pattern("10.0.0.0.1", e));
  CHECK(!gs::parse_ban_pattern("300.1", e));
  CHECK(!gs::parse_ban_pattern("ten", e));
  CHECK(!gs::parse_ban_pattern("10/40", e));
}

TEST(the_list_bans_or_admits_by_its_mode_and_safe_entries_never_refuse) {
  gs::BanList list;
  std::string out;
  CHECK(gs::ban_command(list, "addip 10.0", 0, out) && out == "added 10.0\n");
  CHECK(gs::ban_command(list, "addip 10.0.0.9 safe", 0, out));
  const gn::Address bad = gn::ipv4(10, 0, 3, 4, 27500), good = gn::ipv4(192, 168, 1, 2, 27500);
  const gn::Address kept = gn::ipv4(10, 0, 0, 9, 27500);
  CHECK(gs::ban_refuses(list, 1, bad, 0));
  CHECK(!gs::ban_refuses(list, 1, good, 0));
  CHECK(!gs::ban_refuses(list, 1, kept, 0));  // inside 10.0, but its safe entry keeps it
  CHECK(!gs::ban_refuses(list, 0, bad, 0));   // mode 0: only the list joins
  CHECK(gs::ban_refuses(list, 0, good, 0));
  CHECK(gs::ban_command(list, "removeip 10.0", 0, out) && out == "removed 10.0\n");
  CHECK(gs::ban_command(list, "removeip 10.0.0.1", 0, out));
  CHECK(out.find("not on the list") != std::string::npos);
}

TEST(entries_end_at_their_time_and_the_words_list_and_write_them) {
  gs::BanList list;
  std::string out;
  CHECK(gs::ban_command(list, "addip 1.2.3.4 +60", 1000, out) && out == "added 1.2.3.4 1060\n");
  CHECK(gs::ban_command(list, "addip 5.6 ban 2000", 1000, out));
  CHECK(gs::ban_refuses(list, 1, gn::ipv4(1, 2, 3, 4, 1), 1059));
  CHECK(!gs::ban_refuses(list, 1, gn::ipv4(1, 2, 3, 4, 1), 1060));
  CHECK(gs::ban_command(list, "writeip", 1500, out));
  CHECK(out == "addip 5.6 2000\n");  // the first ran out at 1060
  CHECK(gs::ban_command(list, "listip", 1500, out) && out == "5.6 2000\n1 addresses listed\n");
  CHECK(gs::ban_command(list, "addip nope", 0, out));
  CHECK(out.find("not an address pattern") != std::string::npos);
  CHECK(gs::ban_command(list, "addip 1.2 later", 0, out));
  CHECK(out.find("neither ban, safe") != std::string::npos);
  CHECK(!gs::ban_command(list, "status", 0, out));
}

TEST(a_vip_list_gives_its_addresses_a_level) {
  gs::BanList vips;
  std::string out;
  CHECK(gs::vip_command(vips, "vip_addip 10.1 4", 0, out) && out == "added 10.1 4\n");
  CHECK(gs::vip_command(vips, "vip_addip 10.2", 0, out) && out == "added 10.2 1\n");
  const gs::BanEntry* e = gs::ban_match(vips, gn::ipv4(10, 1, 7, 7, 1), 0);
  CHECK(e != nullptr && e->level == 4);
  CHECK(gs::vip_command(vips, "vip_writeip", 0, out) &&
        out == "vip_addip 10.1 4\nvip_addip 10.2 1\n");
  CHECK(gs::vip_command(vips, "vip_addip 10.3 safe", 0, out));
  CHECK(out.find("is not a level") != std::string::npos);
  CHECK(!gs::vip_command(vips, "addip 10.3", 0, out));  // a ban list's word
  CHECK(!gs::ban_command(vips, "vip_listip", 0, out));
}

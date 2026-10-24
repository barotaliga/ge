// Cvars: kinds, ranges, bound fields, the three tiers, latching and the info strings.
//
// ge:summary Tests the cvar registry: kinds, clamps, bound fields, tiers, latch, info strings
// ge:keywords test cvar cvars bind tier server cheat latch info-string apply_info
// ge:skill console testing
#include "check.hpp"
#include "console/cvar.hpp"

using namespace ge::console;

TEST(kinds_clamp_and_round) {
  Cvars cv;
  Cvar& n = cv.add({.name = "r_scale", .fallback = "1", .min = 0.25, .max = 2});
  CHECK(n.value == "1" && n.number == 1);
  CHECK(cv.set("r_scale", "5", Source::local) == SetResult::changed);
  CHECK(n.value == "2" && n.number == 2);
  CHECK(cv.set("r_scale", "2", Source::local) == SetResult::same);
  Cvar& i = cv.add({.name = "sv_maxclients", .fallback = "8", .kind = CvarKind::integer});
  cv.set(i, "2.5", Source::code);
  CHECK(i.value == "3");
  cv.set(i, "-2.5", Source::code);
  CHECK(i.value == "-3");
  Cvar& b = cv.add({.name = "cl_on", .fallback = "0", .kind = CvarKind::boolean});
  cv.set(b, "7", Source::code);
  CHECK(b.value == "1" && b.on());
  cv.set(b, "false", Source::code);
  CHECK(b.value == "0");
  cv.set(b, "on", Source::code);
  CHECK(b.value == "1");
  Cvar& t = cv.add({.name = "name", .fallback = "player", .kind = CvarKind::text});
  cv.set(t, "a b", Source::local);
  CHECK(t.value == "a b" && t.number == 0);
  CHECK(cv.set("r_scale", "junk", Source::local) == SetResult::changed);
  CHECK(n.value == "0.25");  // not a number reads 0, clamped to the range
  CHECK(cv.set("R_SCALE", "0.5", Source::local) == SetResult::changed);  // any case
  CHECK(n.value == "0.5");
}

TEST(bound_fields_follow_every_set) {
  Cvars cv;
  double d = 0;
  float f = 0;
  int32_t i = 0;
  uint32_t u = 0;
  bool b = false;
  std::string s;
  cv.bind({.name = "a", .fallback = "1.5"}, &d);
  cv.bind({.name = "b", .fallback = "2.5"}, &f);
  cv.bind({.name = "c", .fallback = "7", .kind = CvarKind::integer}, &i);
  cv.bind({.name = "d", .fallback = "30000", .kind = CvarKind::integer, .min = 0}, &u);
  cv.bind({.name = "e", .fallback = "1", .kind = CvarKind::boolean}, &b);
  cv.bind({.name = "f", .fallback = "hi", .kind = CvarKind::text}, &s);
  CHECK(d == 1.5 && f == 2.5f && i == 7 && u == 30000 && b && s == "hi");
  cv.set("a", "-3", Source::local);
  cv.set("c", "9", Source::local);
  cv.set("d", "-5", Source::local);
  cv.set("e", "0", Source::local);
  cv.set("f", "there", Source::local);
  CHECK(d == -3 && i == 9 && u == 0 && !b && s == "there");
}

TEST(revision_moves_only_on_change) {
  Cvars cv;
  Cvar& c = cv.add({.name = "rate", .fallback = "30000"});
  const uint32_t r0 = c.revision;
  cv.set(c, "30000", Source::local);
  CHECK(c.revision == r0);
  cv.set(c, "25000", Source::local);
  CHECK(c.revision == r0 + 1);
}

TEST(tiers_refuse_with_reasons) {
  Cvars client(false), server(true);
  for (Cvars* cv : {&client, &server}) {
    cv->add({.name = "sv_fps", .fallback = "77", .flags = kServer | kServerInfo});
    cv->add({.name = "net_fakelag", .fallback = "0", .flags = kCheat});
    cv->add({.name = "version", .fallback = "1.0", .flags = kReadOnly, .kind = CvarKind::text});
    cv->add({.name = "sensitivity", .fallback = "3", .flags = kArchive});
  }
  CHECK(client.set("sv_fps", "100", Source::local) == SetResult::server);
  CHECK(server.set("sv_fps", "100", Source::local) == SetResult::changed);
  CHECK(client.set("sv_fps", "100", Source::server) == SetResult::changed);
  CHECK(client.set("sensitivity", "9", Source::server) == SetResult::refused);
  CHECK(client.set("version", "2", Source::local) == SetResult::read_only);
  CHECK(client.set("version", "2", Source::code) == SetResult::changed);
  CHECK(server.set("net_fakelag", "100", Source::local) == SetResult::cheat);
  server.set("sv_cheats", "1", Source::local);
  CHECK(server.set("net_fakelag", "100", Source::local) == SetResult::changed);
  // A client's cheats follow the server's info.
  CHECK(client.set("net_fakelag", "100", Source::local) == SetResult::cheat);
  client.apply_info(server.info_string(kServerInfo), Source::server);
  CHECK(client.cheats());
  CHECK(client.set("net_fakelag", "100", Source::local) == SetResult::changed);
  CHECK(!set_result_text(SetResult::server, "sv_fps").empty());
  CHECK(client.set("bad name", "1", Source::local) == SetResult::bad_name);
}

TEST(unknown_names_are_created_only_by_set) {
  Cvars cv;
  CHECK(cv.set("mymod_x", "5", Source::local) == SetResult::unknown);
  CHECK(cv.set("mymod_x", "5", Source::local, true, kArchive) == SetResult::changed);
  const Cvar* c = cv.find("mymod_x");
  CHECK(c != nullptr && c->value == "5" && (c->flags & kCreated) && (c->flags & kArchive));
  // The game registers it later: the spec takes over and the value set is kept.
  Cvar& r = cv.add({.name = "mymod_x", .fallback = "1", .max = 3});
  CHECK(&r == c && r.value == "3" && (r.flags & kCreated) == 0 && (r.flags & kArchive));
}

TEST(latched_values_wait) {
  Cvars cv;
  Cvar& c = cv.add({.name = "sv_mode", .fallback = "1", .flags = kLatch});
  CHECK(cv.set(c, "2", Source::local) == SetResult::latched);
  CHECK(c.value == "1" && c.has_latched);
  CHECK(cv.apply_latched() == 1);
  CHECK(c.value == "2" && !c.has_latched);
}

TEST(info_strings_round_trip) {
  Cvars server(true), client(false);
  server.add({.name = "sv_fps", .fallback = "77", .flags = kServer | kServerInfo});
  server.add({.name = "sv_secret", .fallback = "x", .flags = kServer, .kind = CvarKind::text});
  server.set("sv_fps", "60", Source::local);
  CHECK(server.info_string(kServerInfo) == "\\sv_cheats\\0\\sv_fps\\60");
  client.add({.name = "sv_fps", .fallback = "77", .flags = kServer | kServerInfo});
  CHECK(client.apply_info("\\sv_fps\\60\\sv_extra\\abc", Source::server) == 2);
  CHECK(client.find("sv_fps")->number == 60);
  CHECK(client.find("sv_extra")->value == "abc");
}

TEST(numbers_as_text) {
  double x = 0;
  CHECK(parse_number(" +2.5 ", x) && x == 2.5);
  CHECK(!parse_number("2.5x", x));
  CHECK(!parse_number("", x));
  CHECK(!parse_number("inf", x) && !parse_number("nan", x) && !parse_number("0x10", x));
  CHECK(!parse_number("1e999", x) && !parse_number(".", x) && !parse_number("1e", x));
  CHECK(parse_number("-1.5e-3", x) && x == -0.0015);
  CHECK(parse_number("0.1", x) && x == 0.1);  // correctly rounded: the same double as the literal
  CHECK(number_text(3) == "3");
  CHECK(number_text(-0.0) == "0");
  CHECK(number_text(0.1) == "0.1");
  CHECK(valid_name("r_bloom") && valid_name("+jump") && !valid_name("1a") && !valid_name(""));
}

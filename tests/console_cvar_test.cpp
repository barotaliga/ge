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
  // Cheats turned off again: every cheat is back at its default, on the server and its client.
  server.set("sv_cheats", "0", Source::local);
  CHECK(server.find("net_fakelag")->value == "0");
  client.apply_info(server.info_string(kServerInfo), Source::server);
  CHECK(!client.cheats() && client.find("net_fakelag")->value == "0");
  // A joined game that holds cheats off (Quake II's cheat lock): back to the default, refused,
  // whatever sv_cheats says, until let go.
  server.set("sv_cheats", "1", Source::local);
  CHECK(server.set("net_fakelag", "50", Source::local) == SetResult::changed);
  server.hold_cheats(true);
  CHECK(!server.cheats() && server.find("net_fakelag")->value == "0");
  CHECK(server.set("net_fakelag", "50", Source::local) == SetResult::cheat);
  server.hold_cheats(false);
  CHECK(server.cheats() && server.set("net_fakelag", "50", Source::local) == SetResult::changed);
  server.set("sv_cheats", "0", Source::local);
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

TEST(synonyms_name_a_setting_or_one_word_of_it) {
  Cvars cv(true);
  int32_t players = 8;
  std::string flood = "10 1 1";
  cv.bind({.name = "sv_maxclients", .fallback = "8", .flags = kServer, .kind = CvarKind::integer},
          &players);
  cv.bind({.name = "sv_floodprot", .fallback = "10 1 1", .flags = kServer, .kind = CvarKind::text},
          &flood);
  // A whole synonym is the setting by another name: find and set reach the one cvar.
  CHECK(cv.add_synonym("maxclients", "sv_maxclients"));
  CHECK(cv.find("maxclients") == cv.find("sv_maxclients"));
  CHECK(cv.set("maxclients", "16", Source::local) == SetResult::changed);
  CHECK(players == 16 && cv.find("sv_maxclients")->value == "16");
  // A part synonym is one word of a setting that holds several.
  CHECK(cv.add_synonym("flood_persecond", "sv_floodprot", 1));
  CHECK(cv.find("flood_persecond") == nullptr);  // not a cvar of its own
  const Synonym* s = cv.synonym("flood_persecond");
  CHECK(s != nullptr && cv.synonym_value(*s) == "1");
  CHECK(cv.set("flood_persecond", "7", Source::local) == SetResult::changed);
  CHECK_MSG(flood == "10 7 1", flood);
  CHECK(cv.add_synonym("flood_waitdelay", "sv_floodprot", 4));  // past the end: padded with 0
  CHECK(cv.set("flood_waitdelay", "3", Source::local) == SetResult::changed);
  CHECK_MSG(flood == "10 7 1 0 3", flood);
  // Refused: a name that is a setting of its own, no such setting, a name taken for another.
  CHECK(!cv.add_synonym("sv_floodprot", "sv_maxclients"));
  CHECK(!cv.add_synonym("players", "sv_nothing"));
  CHECK(!cv.add_synonym("maxclients", "sv_floodprot"));
  CHECK(cv.add_synonym("maxclients", "sv_maxclients"));  // the same again is no conflict
  CHECK(cv.synonyms().size() == 3);
}

TEST(a_mapped_synonym_sets_and_reads_its_own_values) {
  Cvars cv;
  int32_t draw2d = 2;
  cv.bind({.name = "scr_draw2d", .fallback = "2", .kind = CvarKind::integer, .min = 0, .max = 3},
          &draw2d);
  // A game's spelling whose 1 is our 2: set by its name, the mapped value lands in ours.
  CHECK(!cv.add_mapped_synonym("r_drawhud", "scr_draw2d", {}));  // no values: refused
  CHECK(cv.add_mapped_synonym("r_drawhud", "scr_draw2d", {{"0", "0"}, {"1", "2"}}));
  CHECK(cv.find("r_drawhud") == nullptr);  // not the setting under another name
  const Synonym* s = cv.synonym("r_drawhud");
  CHECK(s != nullptr && cv.synonym_value(*s) == "1");  // ours 2 read back as its 1
  CHECK(cv.set("r_drawhud", "0", Source::local) == SetResult::changed && draw2d == 0);
  CHECK(cv.synonym_value(*s) == "0");
  CHECK(cv.set("r_drawhud", "1.0", Source::local) == SetResult::changed && draw2d == 2);
  CHECK(cv.set("r_drawhud", "3", Source::local) == SetResult::unmapped && draw2d == 2);
  CHECK(!set_result_text(SetResult::unmapped, "r_drawhud").empty());
  cv.set("scr_draw2d", "3", Source::local);  // a value of ours it has no word for: ours shown
  CHECK(cv.synonym_value(*s) == "3");
  CHECK(Cvars::mapped_words(*s) == "0 (scr_draw2d 0); 1 (scr_draw2d 2)");
  CHECK(cv.mapped_value(*s, "1") == "2" && cv.mapped_value(*s, "7").empty());
}

TEST(meta_cvars_imply_restore_and_refuse_a_loop) {
  Cvars cv;
  double fov = 0;
  cv.bind({.name = "cl_fov", .fallback = "90", .flags = kArchive, .min = 10, .max = 140}, &fov);
  cv.add({.name = "cl_rate", .fallback = "30", .kind = CvarKind::integer});
  cv.add({.name = "cl_mode", .fallback = "0", .kind = CvarKind::integer});
  std::string error;
  CHECK(cv.add_meta("cl_comp", {{"1", {{"cl_fov", "120"}, {"cl_rate", "60"}}}}, error));
  CHECK(cv.find("cl_comp") && (cv.find("cl_comp")->flags & kCreated) != 0);
  // Code's sets are the layer under the metas: they do not make a value explicit.
  cv.set("cl_rate", "40", Source::code);
  cv.set("cl_comp", "1", Source::local);
  CHECK(fov == 120 && cv.find("cl_rate")->value == "60");
  CHECK(cv.find("cl_fov")->implied_by == "cl_comp" && cv.find("cl_fov")->below == "90");
  // An explicit set over an implied value wins and stays when the meta goes off.
  cv.set("cl_fov", "100", Source::local);
  cv.set("cl_comp", "0", Source::local);
  CHECK(fov == 100 && cv.find("cl_rate")->value == "40");
  // A latched member waits like any set, and so does its restore.
  cv.add({.name = "cl_shadow", .fallback = "1", .flags = kLatch, .kind = CvarKind::integer});
  CHECK(cv.add_meta("cl_low", {{"1", {{"cl_shadow", "0"}}}}, error));
  cv.set("cl_low", "1", Source::local);
  CHECK(cv.find("cl_shadow")->value == "1" && cv.find("cl_shadow")->latched == "0");
  cv.apply_latched();
  CHECK(cv.find("cl_shadow")->value == "0");
  cv.set("cl_low", "0", Source::local);
  CHECK(cv.find("cl_shadow")->has_latched && cv.find("cl_shadow")->latched == "1");
  // A loop through another meta's level is refused, named, and changes nothing.
  CHECK(cv.add_meta("cl_a", {{"1", {{"cl_b", "1"}}}}, error));
  CHECK(!cv.add_meta("cl_b", {{"1", {{"cl_a", "1"}}}}, error));
  CHECK_MSG(error == "its levels loop: cl_b 1 > cl_a 1 > cl_b 1 (take one of those includes out)",
            error);
  CHECK(cv.meta("cl_b") == nullptr);
}

// Tests: the mod server's builtin catalogue by number (games/shooters_edge/server/catalogue.hpp):
// every standard row installed under its name, the extension rows grouped and offered, and each
// builtin over the world (bi_world.cpp) and over the settings and the map's flow
// (bi_settings.cpp) doing what its row says: vectors, models and sizes, traces, checkclient, find
// and findradius's chain, droptofloor, pointcontents, aim, nextent, makestatic, spawn parms, the
// dumps and traces, the errors, precaching, cvars, the command buffer, the map change, the frag
// log, infokey, checkextension, the date, pausing and the visible weapons.
//
// ge:summary Tests: every builtin of the catalogue by number over the world and the settings
// ge:keywords test qc builtins catalogue checkextension traceline findradius makestatic cvar
// ge:keywords infokey logfrag calltimeofday setpause precache_vwep_model registercvar
// ge:skill progs shooter testing
#include <cmath>
#include <cstring>
#include <string>

#include "check.hpp"
#include "shooter/entity.hpp"
#include "shooters_edge/server/catalogue.hpp"
#include "shooters_edge_qc_fixture.hpp"

using qcfix::ent;
using qcfix::Fixture;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

bool console_has(const sv::QcServer& s, const std::string& text) {
  for (const std::string& l : s.console)
    if (l.find(text) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(every_row_of_the_catalogue_is_installed_by_number) {
  Fixture fx;
  CHECK_MSG(fx.loaded, fx.error);
  for (int i = 0; i < sv::kCatalogueCount; i++) {
    const sv::CatalogueRow& r = sv::kCatalogue[i];
    const pg::BuiltinSlot& slot = fx.vm().builtins[static_cast<size_t>(r.number)];
    CHECK_MSG(slot.fn != nullptr,
              std::string(r.name) + " (#" + std::to_string(r.number) + ") is not installed");
    CHECK_MSG(std::strcmp(slot.name, r.name) == 0 ||
                  (r.number == 448 && std::strcmp(slot.name, "cvar_string") == 0),
              std::string("builtin #") + std::to_string(r.number) + " is named " + slot.name);
  }
  // The standard numbers 1..82 that the catalogue leaves empty stay empty.
  for (int n : {5, 33, 39, 42, 50, 63, 64, 65, 66, 71})
    CHECK_MSG(fx.vm().builtins[static_cast<size_t>(n)].fn == nullptr,
              "builtin #" + std::to_string(n) + " should be empty");
}

TEST(the_extensions_are_offered_and_checked_without_case) {
  Fixture fx;
  CHECK(static_cast<int>(fx.s->extensions.extensions.size()) == sv::kExtensionCount);
  for (int k = 0; k < sv::kExtensionCount; k++) {
    fx.call(99, {sv::kExtensionNames[k]});
    CHECK_MSG(fx.rf == 1.0f, std::string("checkextension(") + sv::kExtensionNames[k] + ") is 0");
  }
  fx.call(99, {"zq_pause"});
  CHECK(fx.rf == 1.0f);
  fx.call(99, {"DP_NO_SUCH_THING"});
  CHECK(fx.rf == 0.0f);
  fx.call(45, {"pr_checkextension"});
  CHECK(fx.rf == 1.0f);
  const pg::Extension* pause = pg::find_extension(fx.s->extensions, "ZQ_PAUSE");
  CHECK(pause != nullptr && pause->builtins.size() == 1 && pause->builtins[0].number == 531);
  CHECK(pause != nullptr && pause->hooks.size() == 2);
  const pg::Extension* strings = pg::find_extension(fx.s->extensions, "ZQ_QC_STRINGS");
  CHECK(strings != nullptr && strings->builtins.size() == 5);
}

TEST(makevectors_aim_and_the_view_axes) {
  Fixture fx;
  fx.call(1, {sh::Vec3f{0, 90, 0}});
  CHECK(near(fx.g(pg::kGVForward), 0) && near(fx.g(pg::kGVForward + 1), 1));
  CHECK(near(fx.g(pg::kGVRight), 1) && near(fx.g(pg::kGVRight + 1), 0));
  CHECK(near(fx.g(pg::kGVUp + 2), 1));
  fx.call(44, {ent(0), 1000});
  CHECK(near(fx.rv.y, 1));
}

TEST(setmodel_and_setsize) {
  Fixture fx;
  const int e = fx.spawn();
  fx.call(3, {ent(e), "progs/missile.mdl"});
  CHECK(!fx.fault);
  CHECK(fx.f(e, pg::kFModelindex) == 3.0f);  // "", the map, the player, the missile
  CHECK(fx.str_field(e, pg::kFModel) == "progs/missile.mdl");
  fx.call(3, {ent(e), "progs/never.mdl"});
  CHECK(fx.fault);
  CHECK(fx.fault_message.find("no precache") != std::string::npos);
  fx.call(4, {ent(e), sh::Vec3f{-1, -2, -3}, sh::Vec3f{4, 5, 6}});
  const sh::Edict& ed = fx.s->sv.edicts[static_cast<size_t>(e)];
  CHECK(ed.mins.x == -1 && ed.maxs.z == 6 && ed.size.y == 7);
  fx.call(6, {});
  CHECK(fx.fault);
}

TEST(traceline_and_tracebox_meet_the_floor) {
  Fixture fx;
  fx.call(16, {sh::Vec3f{0, 0, 50}, sh::Vec3f{0, 0, -50}, 0, ent(0)});
  CHECK(near(fx.g(pg::kGTraceFraction), 0.5f, 0.01f));
  CHECK(near(fx.g(pg::kGTraceEndpos + 2), 0, 0.1f));
  CHECK(fx.g(pg::kGTracePlaneNormal + 2) == 1.0f);
  CHECK(pg::global_int(fx.vm(), pg::kGTraceEnt) == 0);
  fx.call(16, {sh::Vec3f{0, 0, 50}, sh::Vec3f{0, 0, 60}, 0, ent(0)});
  CHECK(fx.g(pg::kGTraceFraction) == 1.0f);
  fx.call(90, {sh::Vec3f{0, 0, 50}, sh::Vec3f{-16, -16, -16}, sh::Vec3f{16, 16, 16},
               sh::Vec3f{0, 0, -50}, 0, ent(0)});
  CHECK(near(fx.g(pg::kGTraceEndpos + 2), 16, 0.2f));
}

TEST(pointcontents_reads_the_world) {
  Fixture fx;
  fx.call(41, {sh::Vec3f{0, 0, 10}});
  CHECK(fx.rf == static_cast<float>(sh::kContentsEmpty));
  fx.call(41, {sh::Vec3f{0, 0, -10}});
  CHECK(fx.rf == static_cast<float>(sh::kContentsSolid));
}

TEST(find_nextent_and_findradius) {
  Fixture fx;
  fx.call(18, {ent(0), qcfix::raw(pg::kFClassname), "info_player_deathmatch"});
  const int first = fx.re;
  CHECK(first > sh::kMaxClients);
  fx.call(18, {ent(first), qcfix::raw(pg::kFClassname), "info_player_deathmatch"});
  CHECK(fx.re > first);
  fx.call(18, {ent(0), qcfix::raw(pg::kFClassname), "no_such_class"});
  CHECK(fx.re == 0);
  fx.call(47, {ent(0)});
  CHECK(fx.re == first);  // the client slots are not in use
  const int p1 = fx.join(0, "one");
  const int p2 = fx.join(1, "two");
  const sh::Vec3f at = fx.s->sv.edicts[static_cast<size_t>(p1)].origin;
  fx.call(22, {at, 40});
  CHECK(fx.re == p1);
  CHECK(pg::entity_number(fx.vm(),
                          static_cast<int32_t>(*pg::entity_field(fx.vm(), p1, pg::kFChain))) == 0);
  fx.call(22, {sh::Vec3f{0, 0, 24}, 2000});
  // Both players on the chain (the spawn points are not solid and are not on it).
  int seen = 0;
  for (int e = fx.re; e != 0; e = pg::entity_number(fx.vm(), static_cast<int32_t>(*pg::entity_field(
                                                                 fx.vm(), e, pg::kFChain)))) {
    CHECK(e == p1 || e == p2);
    seen++;
  }
  CHECK(seen == 2);
}

TEST(checkclient_finds_a_living_client_and_skips_notarget) {
  Fixture fx;
  const int monster = fx.spawn();
  fx.call(2, {ent(monster), sh::Vec3f{0, 0, 24}});
  fx.call(17, {}, monster);
  CHECK(fx.re == 0);
  const int p1 = fx.join(0, "one");
  fx.s->check_time = -1;
  fx.call(17, {}, monster);
  CHECK(fx.re == p1);
  fx.s->sv.edicts[static_cast<size_t>(p1)].flags |= 128;
  fx.s->check_client = 0;
  fx.s->check_time = -1;
  fx.call(17, {}, monster);
  CHECK(fx.re == 0);
}

TEST(droptofloor_sets_an_entity_on_the_ground) {
  Fixture fx;
  const int e = fx.spawn();
  fx.call(4, {ent(e), sh::Vec3f{-8, -8, 0}, sh::Vec3f{8, 8, 8}});
  fx.call(2, {ent(e), sh::Vec3f{-300, 0, 100}});
  fx.call(34, {}, e);
  CHECK(fx.rf == 1.0f);
  const sh::Edict& ed = fx.s->sv.edicts[static_cast<size_t>(e)];
  CHECK(near(ed.origin.z, 0, 0.1f));
  CHECK((ed.flags & sh::kFlagOnGround) != 0);
  fx.call(2, {ent(e), sh::Vec3f{-300, 0, 600}});
  fx.call(34, {}, e);
  CHECK(fx.rf == 0.0f);  // more than 256 above the floor
}

TEST(makestatic_keeps_a_copy_and_removes_the_entity) {
  Fixture fx;
  const int e = fx.spawn();
  fx.call(3, {ent(e), "progs/player.mdl"});
  fx.call(2, {ent(e), sh::Vec3f{1, 2, 3}});
  fx.call(69, {ent(e)});
  CHECK(fx.s->statics.size() == 1);
  CHECK(fx.s->statics[0].modelindex == 2 && fx.s->statics[0].origin.y == 2);
  CHECK(fx.s->sv.edicts[static_cast<size_t>(e)].free);
  CHECK(fx.events(sv::EventKind::static_entity).size() == 1);
  fx.call(69, {ent(0)});
  CHECK(fx.fault);
}

TEST(setspawnparms_reads_a_clients_saved_parms) {
  Fixture fx;
  const int p = fx.join(0, "one");
  fx.call(78, {ent(p)});
  CHECK(fx.g(pg::kGParm1) == 100.0f);  // the test mod's SetNewParms
  const int other = fx.spawn();
  fx.call(78, {ent(other)});
  CHECK(fx.fault);
}

TEST(the_dumps_and_the_statement_trace_go_to_the_console) {
  Fixture fx;
  const int p = fx.join(0, "dumped");
  fx.call(31, {ent(p)});
  CHECK(console_has(*fx.s, "entity 1:"));
  CHECK(console_has(*fx.s, "netname dumped"));
  fx.call(28, {});
  CHECK(console_has(*fx.s, "classname info_player_deathmatch"));
  fx.call(29, {});
  CHECK(fx.s->tracing && fx.vm().trace != nullptr);
  fx.call(30, {});
  CHECK(!fx.s->tracing && fx.vm().trace == nullptr);
}

TEST(error_stops_the_map_and_objerror_frees_self) {
  Fixture fx;
  fx.call(10, {"broken ", "thing"});
  CHECK(fx.fault);
  CHECK(fx.s->map_stopped);
  CHECK(fx.s->stop_reason.find("broken thing") != std::string::npos);
  Fixture fy;
  const int e = fy.spawn();
  fy.call(11, {"bad entity"}, e);
  CHECK(fy.fault && fy.s->map_stopped);
  CHECK(fy.s->sv.edicts[static_cast<size_t>(e)].free);
}

TEST(precaching_is_for_the_load_only) {
  Fixture fx;
  CHECK(fx.s->models.size() == 4 && fx.s->models[2] == "progs/player.mdl");
  CHECK(fx.s->sounds.size() == 3 && fx.s->sounds[1] == "weapons/rocket.wav");
  fx.call(20, {"progs/late.mdl"});
  CHECK(fx.fault);
  fx.s->loading = true;
  fx.call(20, {"progs/late.mdl"});
  CHECK(!fx.fault && fx.rs == "progs/late.mdl" && fx.s->models.size() == 5);
  fx.call(20, {"progs/late.mdl"});
  CHECK(fx.s->models.size() == 5);  // once
  fx.call(19, {" bad"});
  CHECK(fx.fault);
  fx.call(76, {"misc/two.wav"});  // precache_sound2
  CHECK(fx.s->sounds.size() == 4);
  fx.call(68, {"maps/x.bsp"});
  CHECK(fx.rs == "maps/x.bsp");
}

TEST(cvars_from_the_mod) {
  Fixture fx(sv::kArenaEntities, {"timelimit 7"});
  fx.call(45, {"timelimit"});
  CHECK(fx.rf == 7.0f);
  fx.call(45, {"no_such_cvar"});
  CHECK(fx.rf == 0.0f);
  fx.call(72, {"sv_gravity", "600"});
  CHECK(fx.s->sv.set.move.gravity == 600.0f);
  fx.call(72, {"no_such_cvar", "1"});
  CHECK(console_has(*fx.s, "variable no_such_cvar not found"));
  fx.call(103, {"hostname"});
  CHECK(fx.rs == "unnamed");
  fx.call(448, {"no_such_cvar"});
  CHECK(fx.rs.empty());
  fx.call(93, {"My_Setting", "5"});
  CHECK(fx.rf == 1.0f);
  fx.call(93, {"my_setting", "5"});
  CHECK(fx.rf == 0.0f);
  fx.call(103, {"my_setting"});
  CHECK(fx.rs == "my_setting");  // made with its name as its value
}

TEST(localcmd_and_changelevel_go_through_the_command_buffer) {
  Fixture fx;
  fx.call(46, {"serverinfo status warmup\n"});
  CHECK(fx.s->command_buffer == "serverinfo status warmup\n");
  fx.call(70, {"arena2"});
  fx.call(70, {"arena3"});  // only the first change is queued
  sv::qc_frame(*fx.s, nullptr, 0);
  CHECK(sv::info_value(fx.s->serverinfo, "status") == "warmup");
  CHECK(fx.s->pending_map == "arena2");
  CHECK(fx.s->command_buffer.empty());
}

TEST(logfrag_writes_the_short_and_the_long_lines) {
  Fixture fx;
  const int a = fx.join(0, "alpha");
  const int b = fx.join(1, "beta");
  fx.call(79, {ent(a), ent(b)});
  CHECK(fx.s->frag_log.size() == 1 && fx.s->frag_log[0] == "\\alpha\\beta\\\n");
  sv::server_command(*fx.s, "sv_longfraglog 1");
  fx.call(79, {ent(b), ent(a)});
  CHECK(fx.s->frag_log.size() == 2);
  CHECK(fx.s->frag_log[1].rfind("\\frag\\beta\\alpha\\\\\\Fri Jan 02, 12:00:01 2026\\", 0) == 0);
  fx.call(79, {ent(a), ent(0)});
  CHECK(fx.s->frag_log.size() == 2);  // the world is no client
}

TEST(infokey_reads_server_and_client_info) {
  Fixture fx;
  sv::server_command(*fx.s, "serverinfo hostname arena");
  sv::server_command(*fx.s, "localinfo motd hello");
  fx.call(80, {ent(0), "hostname"});
  CHECK(fx.rs == "arena");
  fx.call(80, {ent(0), "motd"});
  CHECK(fx.rs == "hello");
  const int p = fx.join(0, "named");
  fx.call(80, {ent(p), "name"});
  CHECK(fx.rs == "named");
  fx.call(80, {ent(p), "ip"});
  CHECK(fx.rs == "bot");
  fx.call(80, {ent(p), "*userid"});
  CHECK(fx.rs == "1");
  fx.call(80, {ent(p), "ping"});
  CHECK(fx.rs == "0");
  fx.call(80, {ent(p), "nothing"});
  CHECK(fx.rs.empty());
}

TEST(calltimeofday_hands_the_clock_to_the_mods_hook) {
  Fixture fx;
  // The test mod's worldspawn called it at load: start_hour is the fixed clock's 12.
  const pg::Def* d = pg::find_global(fx.progs, "start_hour");
  CHECK(d != nullptr && fx.g(d->offset) == 12.0f);
  CHECK(fx.s->hook_counts[static_cast<int>(sv::HookId::timeofday)] == 1);
  CHECK(sv::date_text({2026, 1, 2, 3, 4, 5}) == "Fri Jan 02, 03:04:05 2026");
  CHECK(sv::date_text({2024, 2, 29, 0, 0, 0}) == "Thu Feb 29, 00:00:00 2024");
}

TEST(setpause_pauses_and_the_frame_holds_still) {
  Fixture fx;
  fx.join(0, "one");
  fx.call(531, {1});
  CHECK(fx.s->paused);
  CHECK(fx.events(sv::EventKind::pause).size() == 1);
  const double t = fx.s->sv.time;
  sv::qc_frame(*fx.s, nullptr, 0);
  CHECK(fx.s->sv.time == t);
  CHECK(fx.s->paused_seconds > 0);
  fx.call(531, {0});
  CHECK(!fx.s->paused);
}

TEST(precache_vwep_model_lists_visible_weapons) {
  Fixture fx;
  fx.s->loading = true;
  fx.call(532, {"w_rock.mdl"});
  CHECK(!fx.fault && fx.rf == 0.0f);
  fx.call(532, {"w_gren.mdl"});
  CHECK(fx.rf == 1.0f);
  fx.call(532, {"bad name.mdl"});
  CHECK(fx.fault);
  fx.s->loading = false;
  fx.call(532, {"w_late.mdl"});
  CHECK(fx.fault);
}

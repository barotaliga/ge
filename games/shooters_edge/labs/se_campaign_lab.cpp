// LAB `se-campaign`: does a single-player program (the campaign's form, crc 5927) load and run on
// the mod server through its own layout table?
//
// The program is the player's own (`--game`: a release zip, a pack or an installed folder, its
// first pack's `progs.dat`, read by classic-enhance's source and never kept), or the test mod's
// single-player stub when no game is given (the smoke run). Tables: the program (its crc, the
// form it names, its size); the layout check, every engine-side global and field of that form
// against the program's definitions; the builtins it declares by number, each against what the
// server's catalogue installs for its form; the packs (whether the full game's second pack is
// there, which opens episodes 2 to 4). Pictures: the two forms' offsets compared name by name
// (where the single-player table shifts against the multiplayer one), and the autoaim on the
// arena from the side (the line of sight, the target, the aim the builtin hands back).
//
// ge:summary Lab: a single-player program on its layout table: the check, builtins, autoaim
// ge:keywords lab se-campaign single-player campaign layout-table layout-check crc 5927 form
// ge:keywords autoaim sv_aim builtins registered pop-lmp pak1 episodes progs-dat
// ge:skill progs shooter labs
// ge:test tests/progs_layout_test.cpp tests/shooters_edge_qc_single_player_test.cpp
#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "arena_chart.hpp"
#include "classicenhance/source.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "progs/file.hpp"
#include "progs/layout.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/catalogue.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "shooters_edge/sim/arena.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace pg = ge::progs;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace la = shooters_edge::labs;
namespace ce = ge::classicenhance;
using ge::core::num::to_fixed;
using Inputs = std::vector<std::pair<std::string, std::string>>;

// The program to run and where it came from; `packs` the files the packs hold (empty for the stub).
struct Loaded {
  pg::Progs progs;
  std::string from;
  bool pak1 = false;             // a later pack beside the first
  bool pop_lmp = false;          // gfx/pop.lmp in the packs: the full game (`registered` 1)
  std::vector<uint8_t> content;  // every pack's files (empty for the stub)
  ge::vfs::Pak pak;
};

// The packs' maps for the campaign host: a map file read into the spec, its bytes kept until the
// next call.
struct PackMaps {
  const Loaded* l = nullptr;
  std::unique_ptr<sv::BspMap> bsp;
  std::vector<std::string> asked;
};

bool pack_map(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  auto& m = *static_cast<PackMaps*>(ctx);
  m.asked.push_back(name);
  const ge::vfs::PakEntry* e = ge::vfs::pak_find(m.l->pak, ("maps/" + name + ".bsp").c_str());
  if (e == nullptr) {
    error = "no maps/" + name + ".bsp in the packs; next: pass --map with a map the packs hold";
    return false;
  }
  m.bsp = std::make_unique<sv::BspMap>();
  return sv::read_bsp_map(ge::vfs::pak_file(m.l->content, *e), name, m.bsp.get(), &out, error);
}

// The campaign stub's maps: the arena under its world.
bool stub_map(void* ctx, const std::string& name, sv::MapSpec& out, std::string& /*error*/) {
  static_cast<PackMaps*>(ctx)->asked.push_back(name);
  out.models = sv::arena_models();
  out.entities = sv::kStubEntities;
  return true;
}

bool load_program(gl::Lab& lab, Loaded& out) {
  std::string error;
  const std::string& game = lab.arg("game");
  if (game.empty()) {
    out.from = "the test mod's single-player stub";
    if (!sv::build_single_player_stub(out.progs, error)) {
      lab.fail("se-campaign: the stub did not assemble: " + error,
               "next: run tests/shooters_edge_qc_single_player_test.cpp");
      return false;
    }
    return true;
  }
  ce::Source src;
  if (!ce::open_source(game, &src, &error)) {
    lab.fail("se-campaign: " + error, "next: pass --game=<a release zip, a pack or a folder>");
    return false;
  }
  const std::vector<uint8_t>& content = src.content();
  ge::vfs::Pak pak;
  if (!ge::vfs::read_pak(content, &pak, &error)) {
    lab.fail("se-campaign: the pack does not read: " + error, "next: check the --game path");
    return false;
  }
  out.pak1 = !src.later.empty();
  out.pop_lmp = ge::vfs::pak_find(pak, "gfx/pop.lmp") != nullptr;
  out.content = content;
  out.pak = pak;
  const ge::vfs::PakEntry* e = ge::vfs::pak_find(pak, "progs.dat");
  if (e == nullptr) {
    lab.fail("se-campaign: the packs hold no progs.dat", "next: pass the game's own folder");
    return false;
  }
  const std::span<const uint8_t> bytes = ge::vfs::pak_file(content, *e);
  out.from = "progs.dat of " + game;
  if (!pg::load_progs(bytes.data(), bytes.size(), out.progs, error, pg::every_form())) {
    lab.fail("se-campaign: the program was refused: " + error,
             "next: read the refusal; a crc of another form needs that form's table "
             "(plugins/progs/layout.hpp)");
    return false;
  }
  return true;
}

void program_tables(gl::Lab& lab, const Loaded& l) {
  const pg::Progs& p = l.progs;
  const pg::Layout* form = p.layout;
  int builtins = 0;
  for (const pg::Function& f : p.functions) builtins += f.first_statement < 0 ? 1 : 0;
  gl::LabTable t{"the program", {"what", "value"}, {}};
  t.rows = {{"from", l.from},
            {"crc", std::to_string(p.crc)},
            {"form", form != nullptr ? form->name : "none"},
            {"functions", std::to_string(p.functions.size() - 1 - builtins)},
            {"builtins declared", std::to_string(builtins)},
            {"statements", std::to_string(p.statements.size())},
            {"global words", std::to_string(p.globals.size())},
            {"entity words", std::to_string(p.entityfields)}};
  lab.table(t);
  lab.metric("program.crc", p.crc, "crc");
  lab.metric("program.statements", static_cast<double>(p.statements.size()), "count");
  if (form == nullptr) return;

  gl::LabTable check{std::string("the layout check: the ") + form->name + " form",
                     {"table", "name", "type", "offset", "the program has"},
                     {}};
  int declared = 0;
  for (int k = 0; k < 2; k++) {
    const int n = k == 0 ? form->num_globals : form->num_fields;
    for (int i = 0; i < n; i++) {
      const pg::LayoutEntry& e = k == 0 ? form->globals[i] : form->fields[i];
      const pg::Def* d = k == 0 ? pg::find_global(p, e.name) : pg::find_field(p, e.name);
      declared += d != nullptr ? 1 : 0;
      check.rows.push_back(
          {k == 0 ? "global" : "field", e.name, pg::type_name(e.type), std::to_string(e.offset),
           d == nullptr
               ? "(not declared)"
               : std::string(pg::type_name(d->type)) + " at " + std::to_string(d->offset)});
    }
  }
  lab.table(check);
  lab.metric("layout.entries", form->num_globals + form->num_fields, "count");
  lab.metric("layout.declared", declared, "count");
}

void builtin_table(gl::Lab& lab, const sv::QcServer& s) {
  const pg::Progs& p = *s.progs;
  gl::LabTable t{"the builtins the program declares, against the server's table for its form",
                 {"#", "the program's name", "the server installs"},
                 {}};
  int missing = 0;
  for (const pg::Function& f : p.functions) {
    if (f.first_statement >= 0) continue;
    const int n = -f.first_statement;
    const bool in = n < static_cast<int>(s.q.vm.builtins.size()) &&
                    s.q.vm.builtins[static_cast<size_t>(n)].fn != nullptr;
    missing += in ? 0 : 1;
    t.rows.push_back({std::to_string(n), pg::progs_string(p, f.name),
                      in ? s.q.vm.builtins[static_cast<size_t>(n)].name : "(none)"});
  }
  lab.table(t);
  lab.metric("builtins.missing", missing, "count");
}

// The single-player offsets minus the multiplayer ones, name by name, in the single-player order.
void draw_shift(gl::Lab& lab, const Inputs& in) {
  const pg::Layout& sp = pg::kSinglePlayerLayout;
  const pg::Layout& mp = pg::kMultiplayerLayout;
  gl::LineChart c;
  c.frame = {"THE SINGLE-PLAYER TABLE AGAINST THE MULTIPLAYER ONE",
             "every engine-side name both forms have, in the single-player order", "entry",
             "single-player offset - multiplayer offset (words)"};
  gl::Trace globals{"globals", {}, {}}, fields{"fields", {}, {}};
  for (int i = 0; i < sp.num_globals; i++) {
    const pg::LayoutEntry* m = pg::layout_global(mp, sp.globals[i].name);
    if (m == nullptr) continue;
    globals.x.push_back(i);
    globals.y.push_back(sp.globals[i].offset - m->offset);
  }
  for (int i = 0; i < sp.num_fields; i++) {
    const pg::LayoutEntry* m = pg::layout_field(mp, sp.fields[i].name);
    if (m == nullptr) continue;
    fields.x.push_back(i);
    fields.y.push_back(sp.fields[i].offset - m->offset);
  }
  c.traces = {globals, fields};
  c.rules = {0};
  c.spikes = false;
  la::shoot(lab, "layout-shift", gl::line_chart(c),
            "How far each engine-side global and field of the single-player form sits from the "
            "same name in the multiplayer form. Globals: -1 after newmis (absent), then +2 after "
            "deathmatch, coop and teamplay. Fields: -1 after lastruntime (absent), +2 after "
            "punchangle (3 words), +3 after idealpitch. One table per form is what the adapter "
            "and the builtins read, so the shift lives here and nowhere else.",
            "both lines in flat steps only, no step anywhere but at those names; nothing past "
            "the table's end",
            in);
}

// The autoaim on the arena: a shooter, a target ahead and above, the aim handed back.
void autoaim(gl::Lab& lab, const Inputs& in) {
  pg::Progs stub;
  std::string error;
  if (!sv::build_single_player_stub(stub, error)) return;
  sv::QcServerOptions o;
  o.loop.max_edicts = 64;
  std::unique_ptr<sv::QcServer> s = sv::make_qc_server(stub, o, error);
  if (!s) {
    lab.fail("se-campaign: no server: " + error, "next: run the single-player test");
    return;
  }
  sv::MapSpec map;
  map.models = sv::arena_models();
  map.entities = sv::kStubEntities;
  if (!sv::load_map(*s, map, error)) {
    lab.fail("se-campaign: the map did not load: " + error, "next: run the single-player test");
    return;
  }
  const pg::LayoutFields f = pg::kSinglePlayerLayout.f;
  pg::Vm& vm = s->q.vm;
  auto box = [&](sh::Vec3f at, float takedamage) {
    shooters_edge::sim::enter_qc(s->q, s->sv);
    const int e = shooters_edge::sim::qc_spawn_entity(s->q);
    *pg::entity_field(vm, e, f.solid) = pg::as_word(2.0f);
    *pg::entity_field(vm, e, f.takedamage) = pg::as_word(takedamage);
    shooters_edge::sim::flush_to_world(s->q);
    sh::set_size(s->sv, e, {-16, -16, -24}, {16, 16, 32});
    sh::set_origin(s->sv, e, at);
    shooters_edge::sim::refresh_from_world(s->q);
    shooters_edge::sim::leave_qc(s->q);
    return e;
  };
  const sh::Vec3f eye{-416, -256, 24};
  const int shooter = box(eye, 0);
  const float rise = static_cast<float>(lab.arg_num("rise"));
  const sh::Vec3f at{-216, -256, 24 + rise};
  box(at, 2);
  // aim(shooter) with v_forward along +x, as a program would call it after makevectors.
  shooters_edge::sim::enter_qc(s->q, s->sv);
  const int fwd = pg::kSinglePlayerLayout.g.v_forward;
  pg::set_global_float(vm, fwd, 1);
  pg::set_global_float(vm, fwd + 1, 0);
  pg::set_global_float(vm, fwd + 2, 0);
  pg::set_global_int(vm, pg::parm(0), pg::entity_value(vm, shooter));
  pg::set_global_float(vm, pg::parm(1), 1000);
  vm.argc = 2;
  vm.builtins[44].fn(vm);
  const sh::Vec3f aim{pg::global_float(vm, pg::kGlobalReturn),
                      pg::global_float(vm, pg::kGlobalReturn + 1),
                      pg::global_float(vm, pg::kGlobalReturn + 2)};
  shooters_edge::sim::leave_qc(s->q);
  const float pitch =
      static_cast<float>(ge::core::libm::atan2(aim.z, std::sqrt(aim.x * aim.x + aim.y * aim.y))) *
      57.29578f;
  lab.metric("autoaim.pitch", pitch, "deg");
  gl::LabTable t{"the autoaim", {"what", "value"}, {}};
  t.rows = {{"sv_aim", s->cvars.find("sv_aim")->value},
            {"target above the shooter units", to_fixed(rise, 0)},
            {"aim", to_fixed(aim.x, 4) + " " + to_fixed(aim.y, 4) + " " + to_fixed(aim.z, 4)},
            {"pitch up deg", to_fixed(pitch, 2)}};
  lab.table(t);

  gl::LineChart c;
  c.frame = {"AUTOAIM FROM THE SIDE", "the arena, x against z; the target 200 ahead", "x (units)",
             "z (units)"};
  const sh::Vec3f start = eye + sh::Vec3f{0, 0, 20};
  gl::Trace sight{"line of sight", {start.x, start.x + 260}, {start.z, start.z}};
  gl::Trace aimed{
      "aim handed back", {start.x, start.x + 260 * aim.x}, {start.z, start.z + 260 * aim.z}};
  gl::Trace boxes{"shooter and target", {}, {}};
  la::rect(boxes, eye + sh::Vec3f{-16, -16, -24}, eye + sh::Vec3f{16, 16, 32}, 0, 2);
  la::rect(boxes, at + sh::Vec3f{-16, -16, -24}, at + sh::Vec3f{16, 16, 32}, 0, 2);
  c.traces = {boxes, sight, aimed};
  c.spikes = false;
  la::shoot(lab, "autoaim-side", gl::line_chart(c),
            "A single-player program's aim (builtin 44) with the shooter looking level along +x "
            "and a target that takes aimed damage ahead and above: nothing lies on the line of "
            "sight, so the builtin turns onto the target nearest it (within sv_aim's cosine) by "
            "correcting the pitch alone.",
            "the aim line passing through the target box's middle; the heading unchanged (the "
            "aim stays in this x-z plane)",
            in);
}

// The campaign host on the campaign stub: a new game on `first`, the level change its thinker
// asks for, the parms and the runes carried, then a restart.
void campaign_flow(gl::Lab& lab) {
  pg::Progs mod;
  std::string error;
  if (!sv::build_campaign_stub(mod, error)) {
    lab.fail("se-campaign: " + error, "next: run tests/shooters_edge_campaign_test.cpp");
    return;
  }
  PackMaps maps;
  sv::CampaignOptions o;
  o.server.loop.max_edicts = 64;
  o.load = &stub_map;
  o.load_ctx = &maps;
  o.settings = {"skill 1.4"};
  sv::Campaign c;
  if (!sv::campaign_new_game(c, mod, o, "first", error)) {
    lab.fail("se-campaign: the campaign did not start: " + error,
             "next: run tests/shooters_edge_campaign_test.cpp");
    return;
  }
  sv::campaign_join(c, 0, "player");
  const pg::LayoutFields& f = pg::kSinglePlayerLayout.f;
  auto field = [&](int e, int ofs) {
    return pg::as_float(*pg::entity_field(c.server->q.vm, e, ofs));
  };
  gl::LabTable t{
      "the campaign host on the campaign stub",
      {"map", "time at the first frame", "skill", "player health", "maps played", "serverflags"},
      {}};
  auto row = [&] {
    t.rows.push_back({c.history.back(), to_fixed(c.server->sv.time, 4),
                      c.server->cvars.find("skill")->value, to_fixed(field(1, f.health), 0),
                      to_fixed(field(1, f.frags), 0), to_fixed(c.serverflags, 0)});
  };
  row();
  int frames = 0;
  while (c.history.size() < 2 && frames < 400) {
    if (!sv::campaign_frame(c, nullptr, 0, error)) break;
    frames++;
  }
  row();
  sv::server_command(*c.server, "restart");
  sv::campaign_frame(c, nullptr, 0, error);
  row();
  lab.table(t);
  lab.metric("campaign.change_frame", frames, "frames");
  lab.metric("campaign.carried_health", field(1, f.health), "units");
  lab.note(
      "the campaign stub's level change: expected health 100, 75, 75; maps played 0, 1, 1; "
      "serverflags 0, 1, 1; skill 1 throughout; every map's first frame at 1.2");
}

// The player's own program on its own map: the campaign host's map start with the full rules.
void pack_campaign(gl::Lab& lab, const Loaded& l) {
  PackMaps maps;
  maps.l = &l;
  sv::CampaignOptions o;
  o.server.loop.max_edicts = 1024;
  o.load = &pack_map;
  o.load_ctx = &maps;
  o.registered = l.pop_lmp;
  o.settings = {"skill " + lab.arg("skill"), "deathmatch 0"};
  sv::Campaign c;
  std::string error;
  const std::string map = lab.arg("map");
  const bool ok = sv::campaign_new_game(c, l.progs, o, map, error);
  gl::LabTable t{"the campaign host on the player's own map", {"what", "value"}, {}};
  t.rows.push_back({"map", map});
  t.rows.push_back({"started", ok ? "yes" : "no: " + error});
  if (ok) {
    sv::campaign_join(c, 0, "player");
    const int seconds = static_cast<int>(lab.arg_num("seconds"));
    const int frames = static_cast<int>(seconds / sv::kCampaignStep);
    // The player's hands: the forward axis held (and the view's yaw), a packet a frame.
    sh::Packet p;
    for (sh::MoveCmd& cmd : p.cmds) {
      cmd.msec = 14;
      cmd.forward = static_cast<int16_t>(lab.arg_num("forward"));
      cmd.angles.y = static_cast<float>(lab.arg_num("yaw"));
    }
    const sh::Vec3f start = c.server->sv.edicts[1].origin;
    double walked = 0;
    sh::Vec3f last = start;
    for (int i = 0; i < frames && c.server; i++) {
      p.sequence = static_cast<uint32_t>(i + 1);
      if (!sv::campaign_frame(c, &p, 1, error)) break;
      if (!c.server) break;
      const sh::Vec3f now = c.server->sv.edicts[1].origin;
      walked += static_cast<double>(sh::length(now - last));
      last = now;
    }
    if (c.server) {
      const sh::Edict& pl = c.server->sv.edicts[1];
      t.rows.push_back({"the player move", c.server->sv.set.playermove == sh::PlayerMove::original
                                               ? "original"
                                               : "arena"});
      t.rows.push_back({"the player: start, end",
                        to_fixed(start.x, 1) + " " + to_fixed(start.y, 1) + " " +
                            to_fixed(start.z, 1) + " -> " + to_fixed(pl.origin.x, 1) + " " +
                            to_fixed(pl.origin.y, 1) + " " + to_fixed(pl.origin.z, 1)});
      t.rows.push_back({"the player: walked, on the ground",
                        to_fixed(walked, 1) + " units, " +
                            ((pl.flags & sh::kFlagOnGround) != 0 ? "yes" : "no")});
      lab.metric("pack.walked", walked, "units");
    }
    // A console line after it (`changelevel e1m2`): the change, then one frame on the next map.
    if (c.server && !lab.arg("then").empty()) {
      sv::server_command(*c.server, lab.arg("then"));
      if (!sv::campaign_frame(c, nullptr, 0, error))
        t.rows.push_back({"after `" + lab.arg("then") + "`", "failed: " + error});
    }
  }
  if (c.server) {
    const sv::QcServer& s = *c.server;
    const sv::CampaignStats st = sv::campaign_stats(s);
    int live = 0;
    for (int e = 1; e < s.sv.num_edicts; e++)
      if (!s.sv.edicts[static_cast<size_t>(e)].free) live++;
    t.rows.push_back({"registered (the packs carry the full game)", l.pop_lmp ? "1" : "0"});
    t.rows.push_back({"skill", s.cvars.find("skill")->value});
    t.rows.push_back({"entities live", std::to_string(live)});
    t.rows.push_back({"inhibited by skill", std::to_string(s.inhibited)});
    t.rows.push_back({"monsters (killed / total)",
                      to_fixed(st.killed_monsters, 0) + " / " + to_fixed(st.total_monsters, 0)});
    t.rows.push_back({"secrets (found / total)",
                      to_fixed(st.found_secrets, 0) + " / " + to_fixed(st.total_secrets, 0)});
    t.rows.push_back({"models precached", std::to_string(s.models.size() - 1)});
    t.rows.push_back({"sounds precached", std::to_string(s.sounds.size() - 1)});
    t.rows.push_back({"faults", std::to_string(s.q.faults)});
    t.rows.push_back({"last fault", s.q.last_fault.empty() ? "(none)" : s.q.last_fault});
    t.rows.push_back({"server time", to_fixed(s.sv.time, 3)});
    const pg::LayoutFields& f = s.q.layout->f;
    auto field = [&](int ofs) { return pg::as_float(*pg::entity_field(s.q.vm, 1, ofs)); };
    t.rows.push_back({"the player: health, armour, items", to_fixed(field(f.health), 0) + ", " +
                                                               to_fixed(field(f.armorvalue), 0) +
                                                               ", " + to_fixed(field(f.items), 0)});
    t.rows.push_back(
        {"the player's spawn parms 1..4", to_fixed(s.clients[0].spawn_parms[0], 0) + " " +
                                              to_fixed(s.clients[0].spawn_parms[1], 0) + " " +
                                              to_fixed(s.clients[0].spawn_parms[2], 0) + " " +
                                              to_fixed(s.clients[0].spawn_parms[3], 0)});
    std::string history;
    for (const std::string& h : c.history) history += (history.empty() ? "" : ", ") + h;
    t.rows.push_back({"maps started", history});
    lab.metric("pack.monsters", st.total_monsters, "count");
    lab.metric("pack.faults", static_cast<double>(s.q.faults), "count");
  }
  lab.table(t, gl::Det::measured);
}

void run(gl::Lab& lab) {
  Loaded l;
  if (!load_program(lab, l)) return;
  const Inputs in = {{"game", lab.arg("game").empty() ? "(stub)" : "the player's own"},
                     {"rise", lab.arg("rise")}};
  program_tables(lab, l);
  if (l.progs.layout == nullptr) {
    lab.fail("se-campaign: the program names no form (crc " + std::to_string(l.progs.crc) + ")",
             "next: pass a single-player progs");
    return;
  }
  sv::QcServerOptions o;
  o.loop.max_edicts = 1024;
  std::string error;
  std::unique_ptr<sv::QcServer> s = sv::make_qc_server(l.progs, o, error);
  if (!s) {
    lab.fail("se-campaign: the server refused the program: " + error,
             "next: read the error; skill: progs");
    return;
  }
  // The VM and its builtin table are made with a map: the arena under a bare world, so the
  // program's own worldspawn runs (its precaches and light styles) on our server.
  sv::MapSpec map;
  map.models = sv::arena_models();
  map.entities = "{\n\"classname\" \"worldspawn\"\n}\n";
  const bool loaded = sv::load_map(*s, map, error);
  if (!loaded) {
    lab.fail("se-campaign: the world did not spawn: " + error,
             "next: read the error; the console below says what the program printed");
  }
  gl::LabTable world{"the program's worldspawn on the arena", {"what", "value"}, {}};
  world.rows = {{"spawned", loaded ? "yes" : "no"},
                {"models precached", std::to_string(s->models.size() - 1)},
                {"sounds precached", std::to_string(s->sounds.size() - 1)},
                {"faults", std::to_string(s->q.faults)},
                {"last fault", s->q.last_fault.empty() ? "(none)" : s->q.last_fault}};
  lab.table(world);
  lab.metric("world.faults", static_cast<double>(s->q.faults), "count");
  builtin_table(lab, *s);
  if (!lab.arg("game").empty()) {
    gl::LabTable packs{"the packs", {"what", "value"}, {}};
    packs.rows = {{"a later pack beside the first", l.pak1 ? "yes" : "no"},
                  {"gfx/pop.lmp (the full game: episodes 2 to 4)", l.pop_lmp ? "yes" : "no"}};
    lab.table(packs, gl::Det::measured);
  }
  draw_shift(lab, in);
  autoaim(lab, in);
  campaign_flow(lab);
  if (!lab.arg("game").empty()) pack_campaign(lab, l);
}

const gl::LabSpec kSpec{
    "se-campaign",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "plugins/progs' layout tables (the single-player form, crc 5927) and games/shooters_edge/"
    "server's mod server running a single-player program: the mirror, the builtins of that form",
    "progs",
    "Does a single-player program load through its own layout table, and does the server give it "
    "the builtins its form expects (prints without a level, autoaim, no multicast)?",
    {
        {"game", "",
         "the player's classic game: a release zip, a pack or a folder (empty: the stub)"},
        {"rise", "60", "how far above the shooter the autoaim's target stands, units"},
        {"map", "start", "with --game: the map the campaign starts on"},
        {"skill", "1", "with --game: the skill the campaign is played at"},
        {"seconds", "5", "with --game: how long the map runs after the player joins, s"},
        {"then", "", "with --game: a console line run after that (`changelevel e1m2`)"},
        {"forward", "0", "with --game: the forward axis the player holds every frame, units/s"},
        {"yaw", "0", "with --game: the player's view yaw, degrees"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace

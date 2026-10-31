// The campaign saved and loaded (campaign_save.hpp states the chunks and the contract).
//
// Every part is walked by ONE template per chunk, run by the save's Writer to write and by its
// Reader to read, so a field saved is a field loaded. A count read is checked against the live
// server's own sizes (the map was started again with the same limits) before anything is indexed.
//
// ge:summary The campaign's state walked once for the save's writer and reader, chunk by chunk
// ge:keywords campaign_save campaign_load campaign_digest describe chunk edict area-links strings
// ge:skill shooter save
// ge:test tests/shooters_edge_campaign_save_test.cpp
#include "shooters_edge/server/campaign_save.hpp"

#include <algorithm>
#include <cstring>

#include "progs/vm.hpp"
#include "save/file.hpp"
#include "save/io.hpp"
#include "save/xxh64.hpp"

namespace shooters_edge::server {

namespace {

namespace save = ge::save;

constexpr uint32_t kCamp = save::tag("CAMP");
constexpr uint32_t kQsrv = save::tag("QSRV");
constexpr uint32_t kLoop = save::tag("LOOP");
constexpr uint32_t kQcvm = save::tag("QCVM");
constexpr uint16_t kChunkVersion = 1;

// A field added to one of these is a field the save must walk (below): the sizes say so.
static_assert(sizeof(sh::Edict) == 304, "a field added to Edict: walk it in `edict` below");
static_assert(sizeof(sh::ClientSlot) == 112, "a field added to ClientSlot: walk it in `slot`");
static_assert(sizeof(sh::Globals) == 24, "a field added to Globals: walk it in `loop` below");

// The program a save was written by: its crc, its sizes and a hash of its code and initial
// globals.
struct ProgsId {
  int32_t crc = 0;
  uint32_t globals = 0, fields = 0, functions = 0, statements = 0;
  uint64_t code = 0;
  bool operator==(const ProgsId&) const = default;
};

ProgsId progs_id(const pg::Progs& p) {
  ProgsId id;
  id.crc = p.crc;
  id.globals = static_cast<uint32_t>(p.globals.size());
  id.fields = static_cast<uint32_t>(p.entityfields);
  id.functions = static_cast<uint32_t>(p.functions.size());
  id.statements = static_cast<uint32_t>(p.statements.size());
  id.code = save::xxh64(p.statements.data(), p.statements.size() * sizeof(pg::Statement));
  id.code = save::xxh64(p.globals.data(), p.globals.size() * sizeof(uint32_t), id.code);
  return id;
}

template <class Io>
void v3(Io& io, sh::Vec3f& v) {
  io(v.x);
  io(v.y);
  io(v.z);
}

template <class Io>
void cmd(Io& io, sh::MoveCmd& c) {
  io(c.msec);
  v3(io, c.angles);
  io(c.forward);
  io(c.side);
  io(c.up);
  io(c.buttons);
  io(c.impulse);
}

template <class Io>
void edict(Io& io, sh::Edict& e) {
  io(e.free);
  io(e.freetime);
  io(e.last_run);
  io(e.area_node);
  io(e.link_prev);
  io(e.link_next);
  io(e.in_triggers);
  for (sh::Vec3f* v : {&e.origin, &e.velocity, &e.angles, &e.avelocity, &e.mins, &e.maxs, &e.size,
                       &e.absmin, &e.absmax})
    v3(io, *v);
  io(e.movetype);
  io(e.solid);
  io(e.flags);
  io(e.groundentity);
  io(e.owner);
  io(e.nextthink);
  io(e.think);
  io(e.touch);
  io(e.blocked);
  io(e.ltime);
  io(e.modelindex);
  io(e.waterlevel);
  io(e.watertype);
  io(e.teleport_time);
  v3(io, e.v_angle);
  io(e.fixangle);
  io(e.health);
  io(e.button0);
  io(e.button1);
  io(e.button2);
  io(e.impulse);
  io(e.gravity);
  io(e.maxspeed);
  for (sh::Vec3f* v : {&e.movement, &e.oldorigin, &e.view_ofs, &e.movedir, &e.punchangle})
    v3(io, *v);
}

template <class Io>
void slot(Io& io, sh::ClientSlot& c) {
  io(c.active);
  io(c.spectator);
  io(c.bot);
  io(c.old_spectator);
  io(c.last_sequence);
  io(c.any_packet);
  cmd(io, c.last_cmd);
  io(c.jump_held);
  v3(io, c.ground_normal);
  io(c.speed_bank_ms);
  io(c.speed_check_time);
  io(c.last_msec);
  io(c.last_cmd_time);
  cmd(io, c.latest);
  io(c.any_latest);
}

template <class Io>
void extras(Io& io, pg::EntityExtras& x) {
  io(x.has_alpha);
  io(x.alpha);
  io(x.has_colormod);
  io(x.colormod);
}

// A count the reader takes only when it is `expected` (the live server's own).
template <class Io>
bool same_count(Io& io, uint32_t n, size_t expected, const char* what) {
  if (n == expected) return true;
  io.fail("the save holds " + std::to_string(n) + " of " + what + ", this server " +
          std::to_string(expected) + "; next: load it with the build and map it was saved on");
  return false;
}

// A count the reader takes only when it is at most `most`.
template <class Io>
bool at_most(Io& io, int32_t n, int32_t least, int32_t most, const char* what) {
  if (n >= least && n <= most) return true;
  io.fail("the save's " + std::string(what) + " is " + std::to_string(n) + ", outside " +
          std::to_string(least) + ".." + std::to_string(most) +
          "; next: the file is damaged, load another save");
  return false;
}

// CAMP: what the campaign carries.
template <class Io>
void campaign(Io& io, Campaign& c, ProgsId& id) {
  io(id.crc);
  io(id.globals);
  io(id.fields);
  io(id.functions);
  io(id.statements);
  io(id.code);
  io(c.map.name);
  io(c.serverflags);
  io(c.entered_serverflags);
  for (CampaignPlayer& p : c.players) {
    io(p.active);
    io(p.name);
    io(p.carried);
    io(p.parms);
    io(p.entered);
  }
  io(c.settings);
  io(c.history);
}

// QSRV: the mod server's own.
template <class Io>
void mod_server(Io& io, QcServer& s) {
  for (std::string& style : s.lightstyles) io(style);
  for (ClientInfo& c : s.clients) {
    io(c.active);
    io(c.spectator);
    io(c.bot);
    io(c.userid);
    io(c.userinfo);
    io(c.spawn_parms);
    io(c.stuff);
    io(c.drop);
    io(c.address);
    io(c.ping_ms);
  }
  uint32_t n = static_cast<uint32_t>(s.statics.size());
  io.length(n, 1);
  if constexpr (Io::kReading) s.statics.resize(n);
  for (StaticEntity& st : s.statics) {
    io(st.modelindex);
    io(st.frame);
    io(st.colormap);
    io(st.skin);
    v3(io, st.origin);
    v3(io, st.angles);
    extras(io, st.extras);
  }
  n = static_cast<uint32_t>(s.extras.size());
  io.length(n, 1);
  if (!same_count(io, n, s.extras.size(), "the entities' extras")) return;
  for (pg::EntityExtras& x : s.extras) extras(io, x);
  io(s.models);
  io(s.sounds);
  io(s.vwep);
  io(s.serverinfo);
  io(s.localinfo);
  io(s.command_buffer);
  io(s.serverflags);
  io(s.paused_seconds);
  io(s.check_time);
  io(s.check_client);
  io(s.inhibited);
  io(s.next_userid);
  io(s.paused);
}

// LOOP: the loop's server. Entity slots past `num_edicts` are free and at their defaults.
template <class Io>
void loop(Io& io, sh::Server& sv) {
  io(sv.time);
  io(sv.frame);
  io(sv.last_physics);
  io(sv.physics_started);
  io(sv.base_time);
  io(sv.base_frame);
  io(sv.g.time);
  io(sv.g.frametime);
  io(sv.g.self);
  io(sv.g.other);
  io(sv.g.force_retouch);
  io(sv.g.newmis);
  io(sv.touch_stamp);
  const int32_t max = static_cast<int32_t>(sv.edicts.size());
  int32_t live = sv.num_edicts;
  io(live);
  if (!at_most(io, live, 1, max, "entities in use")) return;
  if constexpr (Io::kReading)
    for (int32_t e = live; e < sv.num_edicts; e++) sv.edicts[static_cast<size_t>(e)] = sh::Edict{};
  sv.num_edicts = live;
  for (int32_t e = 0; e < live; e++) {
    sh::Edict& ed = sv.edicts[static_cast<size_t>(e)];
    edict(io, ed);
    if constexpr (Io::kReading) {
      if (!at_most(io, ed.area_node, -1, sh::kAreaNodes - 1, "an entity's area node") ||
          !at_most(io, ed.link_prev, -1, max - 1, "an entity's link") ||
          !at_most(io, ed.link_next, -1, max - 1, "an entity's link"))
        return;
    }
  }
  int32_t nodes = sv.area.count;
  io(nodes);
  if (!same_count(io, static_cast<uint32_t>(nodes), static_cast<size_t>(sv.area.count),
                  "the area tree's nodes"))
    return;
  for (int i = 0; i < nodes; i++) {
    sh::AreaNode& node = sv.area.nodes[i];
    for (sh::AreaList* list : {&node.triggers, &node.solids}) {
      io(list->head);
      io(list->tail);
      if (!at_most(io, list->head, -1, live - 1, "an area list's head") ||
          !at_most(io, list->tail, -1, live - 1, "an area list's tail"))
        return;
    }
  }
  uint32_t n = static_cast<uint32_t>(sv.clients.size());
  io.length(n, 1);
  if (!same_count(io, n, sv.clients.size(), "the client slots")) return;
  for (sh::ClientSlot& c : sv.clients) slot(io, c);
  n = static_cast<uint32_t>(sv.touched.size());
  io.length(n, 4);
  if (!same_count(io, n, sv.touched.size(), "the touch stamps")) return;
  io.raw(sv.touched.data(), sv.touched.size() * sizeof(uint32_t));
}

// A NUL-ended string in a fixed buffer of `size` bytes, as its text.
template <class Io>
void text_in(Io& io, char* buf, size_t size) {
  std::string t;
  if constexpr (!Io::kReading) t.assign(buf, strnlen(buf, size));
  io(t);
  if constexpr (Io::kReading) {
    if (t.size() >= size) {
      io.fail("the save's temporary string is " + std::to_string(t.size()) + " bytes, too long" +
              "; next: the file is damaged, load another save");
      return;
    }
    std::memset(buf, 0, size);
    t.copy(buf, t.size());  // the rest of the buffer stays zero: the string ends there
  }
}

// QCVM: the program's memory, its strings and its generator.
template <class Io>
void program(Io& io, sim::QcProgram& q) {
  pg::Vm& vm = q.vm;
  uint32_t n = static_cast<uint32_t>(vm.globals.size());
  io.length(n, 4);
  if (!same_count(io, n, vm.globals.size(), "the program's globals")) return;
  io.raw(vm.globals.data(), n * sizeof(uint32_t));
  int32_t live = vm.num_entities;
  io(live);
  if (!at_most(io, live, 1, vm.max_entities, "the program's entities")) return;
  const size_t row = static_cast<size_t>(vm.entityfields);
  if constexpr (Io::kReading)
    if (live < vm.num_entities)  // the map's start used more: those rows back to zero
      std::fill(vm.entities.begin() + static_cast<std::ptrdiff_t>(row * static_cast<size_t>(live)),
                vm.entities.begin() +
                    static_cast<std::ptrdiff_t>(row * static_cast<size_t>(vm.num_entities)),
                0u);
  vm.num_entities = live;
  io.raw(vm.entities.data(), row * static_cast<size_t>(live) * sizeof(uint32_t));
  // The engine's strings: their text and where each starts.
  int32_t count = vm.engine_count, used = vm.engine_used;
  io(count);
  io(used);
  if (!at_most(io, count, 0, static_cast<int32_t>(vm.engine_offsets.size()), "engine strings") ||
      !at_most(io, used, 0, static_cast<int32_t>(vm.engine_text.size()), "engine string bytes"))
    return;
  vm.engine_count = count;
  vm.engine_used = used;
  io.raw(vm.engine_text.data(), static_cast<size_t>(used));
  io.raw(vm.engine_offsets.data(), static_cast<size_t>(count) * sizeof(int32_t));
  if constexpr (Io::kReading)
    for (int32_t k = 0; k < count; k++)
      if (!at_most(io, vm.engine_offsets[static_cast<size_t>(k)], 0, std::max(used - 1, 0),
                   "an engine string's start"))
        return;
  for (char* t : vm.temps) text_in(io, t, pg::kTempStringBytes);
  io(vm.temp_next);
  if (!at_most(io, vm.temp_next, 0, pg::kTempStrings - 1, "the next temporary")) return;
  for (int k = 0; k < pg::kBuiltinTempStrings; k++)
    text_in(io, vm.builtin_temps.data() + static_cast<size_t>(k) * pg::kTempStringBytes,
            pg::kTempStringBytes);
  io(vm.builtin_temp_next);
  if (!at_most(io, vm.builtin_temp_next, 0, pg::kBuiltinTempStrings - 1, "the next builtin text"))
    return;
  n = static_cast<uint32_t>(vm.zone.size());
  io.length(n, 4);
  if (!same_count(io, n, vm.zone.size(), "the zone's strings")) return;
  for (std::string& z : vm.zone) io(z);
  io(vm.zone_used);
  if (!same_count(io, static_cast<uint32_t>(vm.zone_used.size()), vm.zone.size(), "the zone's use"))
    return;
  io(vm.words.count);
  io(vm.words.at);
  io(vm.words.text);
  if (!at_most(io, vm.words.count, 0, pg::kMaxWords, "the tokenized words")) return;
  io(q.live);
  if (!same_count(io, static_cast<uint32_t>(q.live.size()), static_cast<size_t>(vm.max_entities),
                  "the live entities"))
    return;
  io(q.rng);
}

std::string title_of(const QcServer& s) {
  const int32_t v = static_cast<int32_t>(*pg::entity_field(s.q.vm, 0, s.q.layout->f.message));
  const char* t = pg::vm_string(s.q.vm, v);
  return t == nullptr || *t == '\0' ? s.map_name : t;
}

}  // namespace

std::string campaign_save_problem(const Campaign& c) {
  if (!c.server) return "no map is running; next: start a game with `map <name>` first";
  const QcServer& s = *c.server;
  if (!s.q.layout->single_player)
    return "the program is not a single-player one; next: save a campaign, not a match";
  for (const char* rule : {"deathmatch", "coop"})
    if (s.cvars.find(rule)->on())
      return std::string("a game with ") + rule +
             " on does not save; next: save a single-player game (`" + rule + " 0`)";
  for (int i = 1; i < sh::kMaxClients; i++)
    if (s.clients[i].active) return "another player is in the game; next: save alone";
  if (!s.clients[0].active) return "nobody is in the game; next: join it first";
  if (s.sv.edicts[1].health <= 0) return "the player is dead; next: respawn, then save";
  return "";
}

bool campaign_save(const Campaign& c, std::vector<uint8_t>& out, std::string& error,
                   int64_t saved_unix) {
  error = campaign_save_problem(c);
  if (!error.empty()) {
    error = "save: " + error;
    return false;
  }
  QcServer& s = *c.server;
  // The settings as the next map would carry them: every cvar changed from its default.
  Campaign& carried = const_cast<Campaign&>(c);
  std::vector<std::pair<std::string, std::string>> settings;
  for (const ge::console::Cvar* v : s.cvars.sorted())
    if (!v->is_default()) settings.emplace_back(v->name, v->value);
  std::swap(carried.settings, settings);

  const CampaignStats st = campaign_stats(s);
  save::Meta meta;
  meta.game = kSaveGame;
  meta.game_version = kSaveVersion;
  meta.title = title_of(s) + ", kills " + std::to_string(static_cast<int>(st.killed_monsters)) +
               "/" + std::to_string(static_cast<int>(st.total_monsters));
  meta.place = c.map.name;
  meta.play_s = s.sv.time;
  meta.saved_unix = saved_unix;
  meta.setup = "map " + c.map.name;
  save::FileWriter w(out, meta);
  ProgsId id = progs_id(*c.progs);
  campaign(w.chunk(kCamp, 0, kChunkVersion), carried, id);
  std::swap(carried.settings, settings);
  mod_server(w.chunk(kQsrv, 0, kChunkVersion), s);
  loop(w.chunk(kLoop, 0, kChunkVersion), s.sv);
  program(w.chunk(kQcvm, 0, kChunkVersion), s.q);
  std::string why;
  if (!w.finish(&why)) {
    error = "save: " + why;
    return false;
  }
  return true;
}

bool campaign_load(Campaign& c, const pg::Progs& progs, const CampaignOptions& opt,
                   std::span<const uint8_t> bytes, std::string_view name, std::string& error) {
  c.server.reset();
  save::FileReader f;
  if (!f.open(bytes, name, kSaveGame)) {
    error = f.problem();
    return false;
  }
  if (f.meta().game_version > kSaveVersion) {
    error = std::string(name) + ": written by a newer game (save version " +
            std::to_string(f.meta().game_version) + "); next: load it with that build";
    return false;
  }
  // Each chunk is read where it belongs, in this order, and every one must read to its end.
  const auto chunk = [&](uint32_t tag, auto&& walk) {
    const save::ChunkInfo* info = f.find(tag);
    if (info == nullptr || info->version > kChunkVersion) {
      error = std::string(name) + ": chunk " + save::tag_name(tag) +
              (info == nullptr ? " missing" : " newer than this build reads") +
              "; next: load a save this build wrote";
      return false;
    }
    save::Reader r = f.reader(*info);
    walk(r);
    if (r.ok() && !r.at_end())
      r.fail("the chunk has bytes left over once read; next: load a save this build wrote");
    if (!r.ok()) {
      error = std::string(name) + ": chunk " + save::tag_name(tag) + " at byte " +
              std::to_string(r.offset()) + ": " + r.problem();
      if (r.problem().find("next:") == std::string::npos)
        error += "; next: the file is damaged or of another build, load another save";
      return false;
    }
    return true;
  };
  ProgsId id;
  c.history.clear();
  for (CampaignPlayer& p : c.players) p = CampaignPlayer{};
  if (!chunk(kCamp, [&](save::Reader& r) { campaign(r, c, id); })) return false;
  if (!(id == progs_id(progs))) {
    error = std::string(name) +
            ": written by another program (its code differs from the one loaded); next: load it "
            "with the program it was saved with";
    return false;
  }
  const std::string map = c.map.name;
  const float entered = c.entered_serverflags;
  std::vector<std::string> history = std::move(c.history);
  if (!campaign_reopen(c, progs, opt, map, error)) {
    c.server.reset();
    return false;
  }
  history.push_back(c.history.back());
  c.history = std::move(history);
  c.entered_serverflags = entered;
  QcServer& s = *c.server;
  const bool ok = chunk(kQsrv, [&](save::Reader& r) { mod_server(r, s); }) &&
                  chunk(kLoop, [&](save::Reader& r) { loop(r, s.sv); }) &&
                  chunk(kQcvm, [&](save::Reader& r) { program(r, s.q); });
  if (!ok) {
    c.server.reset();
    return false;
  }
  return true;
}

uint64_t campaign_digest(const QcServer& s) {
  const pg::Vm& vm = s.q.vm;
  uint64_t h = sh::server_hash(s.sv);
  h = save::xxh64(vm.globals.data(), vm.globals.size() * sizeof(uint32_t), h);
  const size_t words = static_cast<size_t>(vm.entityfields) * static_cast<size_t>(vm.num_entities);
  return save::xxh64(vm.entities.data(), words * sizeof(uint32_t), h);
}

}  // namespace shooters_edge::server

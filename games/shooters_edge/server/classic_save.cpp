// The classic text save (classic_save.hpp states the text and the load).
//
// ge:summary The classic text save written from the program's state and read back over a map
// ge:keywords classic_save_text classic_load_text sav header-lines entity-blocks relink
// ge:skill shooter progs save
// ge:test tests/shooters_edge_classic_save_test.cpp
#include "shooters_edge/server/classic_save.hpp"

#include <algorithm>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "progs/entity_text.hpp"
#include "progs/vm.hpp"
#include "shooter/area.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/server/campaign_save.hpp"
#include "shooters_edge/sim/qc_program.hpp"

namespace shooters_edge::server {

namespace {

namespace num = ge::core::num;

constexpr int kParms = 16;
constexpr int kHeaderLines = 1 + 1 + kParms + 1 + 1 + 1 + kLightStyles;
constexpr const char* kUnsetStyle = "m";

// A float definition that is one part of a vector (`origin_x`): written with the vector.
bool vector_part(const pg::Progs& p, const pg::Def& d, bool field) {
  if ((d.type & pg::kTypeMask) != pg::kTypeFloat) return false;
  const std::string name = pg::progs_string(p, d.name);
  if (name.size() < 3 || name[name.size() - 2] != '_') return false;
  const char axis = name.back();
  if (axis != 'x' && axis != 'y' && axis != 'z') return false;
  const std::string base = name.substr(0, name.size() - 2);
  const pg::Def* v = field ? pg::find_field(p, base.c_str()) : pg::find_global(p, base.c_str());
  return v != nullptr && (v->type & pg::kTypeMask) == pg::kTypeVector;
}

void pair(std::string& out, const char* name, const std::string& value) {
  out += '"';
  out += name;
  out += "\" \"";
  out += value;
  out += "\"\n";
}

std::string comment_of(const QcServer& s) {
  const pg::Vm& vm = s.q.vm;
  const char* t =
      pg::vm_string(vm, static_cast<int32_t>(*pg::entity_field(vm, 0, s.q.layout->f.message)));
  std::string c = t == nullptr || *t == '\0' ? s.map_name : t;
  const CampaignStats st = campaign_stats(s);
  c += " kills:" + std::to_string(static_cast<int>(st.killed_monsters)) + "/" +
       std::to_string(static_cast<int>(st.total_monsters));
  for (char& ch : c)
    if (ch == ' ' || ch == '\n' || ch == '\r') ch = '_';
  return c;
}

bool read_float(const std::string& line, float& out) {
  const char* p = line.c_str();
  char* end = nullptr;
  const double d = std::strtod(p, &end);
  if (end == p) return false;
  out = static_cast<float>(d);
  return true;
}

// A save from an engine that numbers its entities right after one player (the world, the player,
// then the map's from 2) is moved onto this server's numbering (the map's from `first`): every
// block from 2 on moved up, and every entity value in the blocks and the globals with it. Told
// apart by a block among 2 .. first - 1 that is not a player's.
void renumber(const pg::Progs& p, std::vector<pg::EntityBlock>& blocks, int first) {
  const int shift = first - 2;
  bool compact = false;
  for (size_t i = 3; i < blocks.size() && i < static_cast<size_t>(first) + 1 && !compact; i++) {
    const char* cn = pg::block_value(blocks[i], "classname");
    compact = cn != nullptr && std::string_view(cn) != "player";
  }
  if (!compact || shift <= 0) return;
  for (pg::EntityBlock& b : blocks) {
    const bool global = &b == &blocks[0];
    for (auto& [key, value] : b.pairs) {
      const pg::Def* d = global ? pg::find_global(p, key.c_str()) : pg::find_field(p, key.c_str());
      if (d == nullptr || (d->type & pg::kTypeMask) != pg::kTypeEntity) continue;
      const int n = std::atoi(value.c_str());
      if (n >= 2) value = std::to_string(n + shift);
    }
  }
  blocks.insert(blocks.begin() + 3, static_cast<size_t>(shift), pg::EntityBlock{});
}

}  // namespace

bool classic_save_text(const Campaign& c, std::string& out, std::string& error) {
  error = campaign_save_problem(c);
  if (!error.empty()) {
    error = "save: " + error;
    return false;
  }
  QcServer& s = *c.server;
  sim::enter_qc(s.q, s.sv);  // the program's copy of the world brought up to the loop's
  sim::leave_qc(s.q);
  const pg::Vm& vm = s.q.vm;
  const pg::Progs& p = *s.progs;
  out.clear();
  out += std::to_string(kClassicSaveVersion) + "\n";
  out += comment_of(s) + "\n";
  for (const float parm : s.clients[0].spawn_parms) out += num::to_string(parm) + "\n";
  out += std::to_string(s.cvars.find("skill")->integer()) + "\n";
  out += c.map.name + "\n";
  out += num::to_string(s.sv.time) + "\n";
  for (const std::string& style : s.lightstyles)
    out += (style.empty() ? kUnsetStyle : style) + "\n";
  out += "{\n";
  for (const pg::Def& d : p.globaldefs) {
    if ((d.type & pg::kTypeSaved) == 0 || vector_part(p, d, false)) continue;
    const char* name = pg::progs_string(p, d.name);
    if (*name == '\0') continue;
    pair(out, name, pg::value_text(vm, d.type, &vm.globals[d.offset]));
  }
  out += "}\n";
  for (int e = 0; e < s.sv.num_edicts; e++) {
    out += "{\n";
    if (e == 0 || !s.sv.edicts[static_cast<size_t>(e)].free) {
      for (const pg::Def& d : p.fielddefs) {
        const char* name = pg::progs_string(p, d.name);
        if (*name == '\0' || vector_part(p, d, true)) continue;
        const uint32_t* w = pg::entity_field(vm, e, d.offset);
        if (pg::value_is_zero(d.type, w)) continue;
        pair(out, name, pg::value_text(vm, d.type, w));
      }
    }
    out += "}\n";
  }
  return true;
}

bool classic_load_text(Campaign& c, const pg::Progs& progs, const CampaignOptions& opt,
                       std::string_view text, std::string_view name, const std::string& player,
                       std::string& error) {
  c.server.reset();
  const std::string where(name);
  const auto refuse = [&](int line, const std::string& what) {
    error = where + ", line " + std::to_string(line) + ": " + what +
            "; next: the file is damaged or not a save, load another";
    c.server.reset();
    return false;
  };
  // The header, a line an item.
  std::vector<std::string> lines;
  size_t at = 0;
  while (lines.size() < static_cast<size_t>(kHeaderLines) && at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    std::string line(text.substr(at, end - at));
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(std::move(line));
    at = end + 1;
  }
  if (lines.size() < static_cast<size_t>(kHeaderLines))
    return refuse(static_cast<int>(lines.size()) + 1, "the text ends inside the header");
  if (std::atoi(lines[0].c_str()) != kClassicSaveVersion)
    return refuse(1, "version " + lines[0] + ", not " + std::to_string(kClassicSaveVersion));
  float parms[kParms];
  for (int k = 0; k < kParms; k++)
    if (!read_float(lines[2 + static_cast<size_t>(k)], parms[k]))
      return refuse(3 + k, "spawn parm " + std::to_string(k + 1) + " is not a number");
  float skill = 0, time_f = 0;
  if (!read_float(lines[18], skill)) return refuse(19, "the skill is not a number");
  const std::string map = lines[19];
  if (map.empty()) return refuse(20, "no map name");
  if (!read_float(lines[20], time_f) || time_f < 0) return refuse(21, "the time is not a time");
  const double time = std::strtod(lines[20].c_str(), nullptr);  // the server's own: a double
  std::vector<pg::EntityBlock> blocks;
  const std::string rest(text.substr(std::min(at, text.size())));
  if (!pg::parse_entity_text(rest.c_str(), blocks, error)) return refuse(kHeaderLines, error);
  if (blocks.size() < 3) return refuse(kHeaderLines + 1, "no globals, world and player blocks");

  // The map started again under the text's skill, nobody in it.
  c.history.clear();
  for (CampaignPlayer& pl : c.players) pl = CampaignPlayer{};
  c.settings = {{"deathmatch", "0"},
                {"coop", "0"},
                {"skill", std::to_string(static_cast<int>(num::round(skill)))}};
  c.serverflags = 0;
  if (!campaign_reopen(c, progs, opt, map, error)) {
    c.server.reset();
    return false;
  }
  QcServer& s = *c.server;
  sh::Server& sv = s.sv;
  pg::Vm& vm = s.q.vm;
  const int first =
      sv.set.first_dynamic > sv.set.maxclients ? sv.set.first_dynamic : sv.set.maxclients + 1;
  renumber(progs, blocks, first);
  const size_t slots = std::max(blocks.size() - 1, static_cast<size_t>(first));
  if (slots > sv.edicts.size())
    return refuse(kHeaderLines + 1, std::to_string(slots) + " entities, more than the server's " +
                                        std::to_string(sv.edicts.size()));

  sim::enter_qc(s.q, sv);
  vm.world_locked = false;
  for (int e = 1; e < sv.num_edicts; e++)
    if (!sv.edicts[static_cast<size_t>(e)].free) sim::qc_free_entity(s.q, e);
  for (size_t e = 1; e < sv.edicts.size(); e++) {
    sv.edicts[e] = sh::Edict{};
    s.q.live[e] = 0;
  }
  sv.area = sh::create_area_tree(sv.models[1].mins, sv.models[1].maxs);
  sv.num_edicts = static_cast<int>(slots);
  vm.num_entities = sv.num_edicts;
  bool ok = true;
  for (size_t i = 0; i < slots && ok; i++) {
    static const pg::EntityBlock kFree;
    const pg::EntityBlock& b = i + 1 < blocks.size() ? blocks[i + 1] : kFree;
    const int e = static_cast<int>(i);
    pg::clear_entity(vm, e);
    s.extras[i] = pg::EntityExtras{};
    if (e > 0 && b.pairs.empty()) continue;  // a free slot
    sv.edicts[i].free = false;
    s.q.live[i] = 1;
    ok = pg::apply_entity_block(vm, e, b, s.extras[i], s.map_messages, error);
  }
  if (ok) ok = pg::apply_global_block(vm, blocks[0], s.map_messages, error);
  if (ok) {
    sim::flush_to_world(s.q);
    // Linked again unless its box was never placed (the saved absmin and absmax both zero).
    const auto placed = [&](int e) {
      const uint32_t* lo = pg::entity_field(vm, e, s.q.layout->f.absmin);
      const uint32_t* hi = pg::entity_field(vm, e, s.q.layout->f.absmax);
      return !(pg::value_is_zero(pg::kTypeVector, lo) && pg::value_is_zero(pg::kTypeVector, hi));
    };
    for (int e = 1; e < sv.num_edicts; e++)
      if (!sv.edicts[static_cast<size_t>(e)].free && placed(e)) sh::link_edict(sv, e, false);
  }
  vm.world_locked = true;
  sim::leave_qc(s.q);
  if (!ok) return refuse(kHeaderLines + 1, error);

  // The time, the light styles, the player in his slot as saved.
  sv.time = time;
  sv.g.time = pg::global_float(vm, s.q.layout->g.time);  // the program's, as saved
  sv.g.frametime = pg::global_float(vm, s.q.layout->g.frametime);
  sv.last_physics = time;
  sv.base_time = time;
  sv.base_frame = sv.frame;
  for (int k = 0; k < kLightStyles; k++) {
    const std::string& style = lines[21 + static_cast<size_t>(k)];
    s.lightstyles[k] = style == kUnsetStyle ? "" : style;
  }
  ClientInfo& ci = s.clients[0];
  ci = ClientInfo{};
  ci.active = true;
  ci.userid = s.next_userid++;
  ci.userinfo.emplace_back("name", player);
  std::copy(std::begin(parms), std::end(parms), std::begin(ci.spawn_parms));
  sh::ClientSlot& slot = sv.clients[0];
  slot = sh::ClientSlot{};
  slot.active = true;
  slot.speed_check_time = time;
  slot.last_cmd_time = time;
  if (sv.edicts[1].free) return refuse(kHeaderLines + 2, "the player's entity is free");
  CampaignPlayer& pl = c.players[0];
  pl.active = true;
  pl.name = player;
  std::copy(std::begin(parms), std::end(parms), std::begin(pl.parms));
  std::copy(std::begin(parms), std::end(parms), std::begin(pl.entered));
  pl.carried = true;
  const int flags = s.q.layout->g.serverflags;
  c.serverflags = flags >= 0 ? pg::global_float(vm, flags) : 0;
  c.entered_serverflags = c.serverflags;
  s.serverflags = c.serverflags;
  return true;
}

}  // namespace shooters_edge::server

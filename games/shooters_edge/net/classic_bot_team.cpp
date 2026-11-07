// A team of bots on any classic server (classic_bot_team.hpp).
//
// ge:summary A team of bots on any classic server: N fighters on N UDP clients, one team, a console
// ge:keywords bot-team bot_team_tick bot_team_command seat set-up ready death-line mount
// ge:skill bots net shooter
#include "shooters_edge/net/classic_bot_team.hpp"

#include <cstdio>
#include <fstream>
#include <memory>
#include <utility>

#include "core/num_text.hpp"
#include "progs/file.hpp"
#include "progs/text.hpp"
#include "shooter/locs.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/bots/talk.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::net {

namespace {

namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;

constexpr double kHz = 1000.0 / bt::kPilotMsec;
constexpr int64_t kReadyAfterUs = 2000000;  // set up this long before it says `ready`

// The game folders mounted for reading the map and its place names: mounted once per game folder
// (the packs are read whole), and kept.
struct Mount {
  std::string basedir, gamedir;
  bool open = false;
  cl::SearchPath files;
};

std::vector<uint8_t> read_from(Mount& m, const std::string& gamedir, const std::string& path) {
  std::vector<uint8_t> store;
  if (m.basedir.empty()) return store;
  if (!m.open || m.gamedir != gamedir) {
    std::string why;
    m.files = {};
    m.open = cl::open_game_dir(m.basedir, gamedir, m.files, why);
    // A server's game folder the player does not have (a match mod's): the base game's alone.
    if (!m.open) {
      m.files = {};
      m.open = cl::open_game_dir(m.basedir, "", m.files, why);
    }
    m.gamedir = gamedir;
    if (!m.open) return store;
  }
  const std::span<const uint8_t> got = cl::find_file(m.files, path, store);
  return std::vector<uint8_t>(got.begin(), got.end());
}

// The mount every team shares across its bots' clients (one per team).
std::shared_ptr<Mount>& mount_of(const BotTeam& t) {
  static std::vector<std::pair<const BotTeam*, std::shared_ptr<Mount>>> mounts;
  for (auto& [team, m] : mounts)
    if (team == &t) return m;
  mounts.emplace_back(&t, std::make_shared<Mount>());
  return mounts.back().second;
}

void say(BotTeam& t, const std::string& line) { t.log.push_back(line); }

// A server's line as plain text: the coloured characters (bit 7) as their plain ones, the gold
// digits and brackets as digits and brackets.
std::string plain(const std::string& line) {
  std::string out;
  for (const char ch : line) {
    auto c = static_cast<unsigned char>(ch);
    if (c >= 0x92 && c <= 0x9b)
      c = static_cast<unsigned char>('0' + (c - 0x92));
    else if (c == 0x90)
      c = '[';
    else if (c == 0x91)
      c = ']';
    else if (c >= 0x80)
      c &= 0x7f;
    if (c >= 32 || c == '\n') out.push_back(static_cast<char>(c));
  }
  return out;
}

std::string stem_of(const std::string& file) {
  const size_t slash = file.rfind('/');
  std::string s = slash == std::string::npos ? file : file.substr(slash + 1);
  const size_t dot = s.rfind('.');
  return dot == std::string::npos ? s : s.substr(0, dot);
}

// The map the server named, loaded from the packs and spawned locally for its nav.
bool load_map(BotTeam& t, const ClassicClientEnd& c, const std::string& file) {
  t.map = file;
  t.map_error.clear();
  t.nav.reset();
  t.local.reset();
  t.loaded.reset();
  t.movers_named = false;
  t.plan_said = false;
  for (auto& s : t.seats) s->set_up = false;
  Mount& m = *mount_of(t);
  const std::vector<uint8_t> bytes =
      t.cfg.read_file ? t.cfg.read_file(c.gamedir, file) : read_from(m, c.gamedir, file);
  if (bytes.empty()) {
    t.map_error = "no " + file + " in " + t.cfg.basedir + " (game folder " + c.gamedir +
                  " over the base game's); next: pass the folder holding the player's own packs";
    say(t, "bots: " + t.map_error);
    return false;
  }
  std::string error;
  const std::string name = stem_of(file);
  t.loaded = cl::map_from_bytes(bytes, name, error);
  if (!t.loaded) {
    t.map_error = file + ": " + error;
    say(t, "bots: " + t.map_error);
    return false;
  }
  static pg::Progs progs;
  static bool compiled = false;
  if (!compiled && !cl::baseline_progs(progs, error)) {
    t.map_error = "the baseline mod: " + error;
    say(t, "bots: " + t.map_error);
    return false;
  }
  compiled = true;
  sv::QcServerOptions so;
  so.seed = t.cfg.seed;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(progs, so, error);
  if (!qc) {
    t.map_error = error;
    return false;
  }
  t.local = std::make_unique<sv::RulesRunner>(sv::qc_runner(std::move(qc)));
  t.local->console("deathmatch 1");
  t.local->console("teamplay 1");
  if (!t.local->start(t.loaded->spec, error)) {
    t.map_error = file + " did not spawn: " + error;
    say(t, "bots: " + t.map_error);
    return false;
  }
  sv::QcServer& q = *sv::qc_of(*t.local);
  sv::use_map_sets(q, t.loaded->bsp);
  auto nav = std::make_unique<TeamNav>();
  if (t.cfg.locs) {
    const std::string text = t.cfg.locs(name);
    if (!text.empty() && sh::parse_locs(text, nav->given) > 0) nav->given.from = "the given file";
  } else if (m.open) {
    const cl::GameAdapter& game = cl::classic_adapter();
    game.open_locations(game, m.files, name, nav->given);
  }
  if (!bt::build_map_nav(q, t.loaded->spec, "info_player_deathmatch", nav->nav, error)) {
    t.map_error = file + "'s nav: " + error;
    say(t, "bots: " + t.map_error);
    return false;
  }
  t.nav = std::move(nav);
  t.maps++;
  say(t, "bots: on " + file + ": " + std::to_string(t.nav->nav.graph.nodes.size()) +
             " nav nodes, " + std::to_string(t.nav->nav.movers.size()) + " movers, " +
             std::to_string(t.nav->given.points.size()) + " place names");
  return true;
}

// The nav's movers named by the server's entity numbers: the entity whose baseline carries the
// same brush model as the local copy's.
void name_movers(BotTeam& t, const ClassicClientEnd& c) {
  const sv::QcServer& q = *sv::qc_of(*t.local);
  const auto renumber = [&](sh::NavMover& mv) {
    if (mv.ent <= 0 || static_cast<size_t>(mv.ent) >= q.sv.edicts.size()) return;
    const int mi = q.sv.edicts[static_cast<size_t>(mv.ent)].modelindex;
    if (mi <= 0 || static_cast<size_t>(mi) >= q.models.size()) return;
    const std::string& model = q.models[static_cast<size_t>(mi)];
    for (const EntityState& b : c.baselines)
      if (b.modelindex > 0 && static_cast<size_t>(b.modelindex) < c.models.size() &&
          c.models[static_cast<size_t>(b.modelindex)] == model) {
        mv.ent = b.number;
        return;
      }
  };
  for (sh::NavMover& mv : t.nav->nav.graph.movers) renumber(mv);
  for (sh::NavMover& mv : t.nav->nav.movers) renumber(mv);
  t.movers_named = true;
}

void set_up(BotTeam& t, BotSeat& s) {
  const ClassicClientEnd& c = s.c;
  std::string error;
  s.mirror = std::make_unique<ClientMirror>();
  if (!mirror_start(*s.mirror, t.loaded->spec.models, c, error)) {
    say(t, s.name + ": " + error);
    s.mirror.reset();
    return;
  }
  s.mirror->baselines = true;
  mirror_update(*s.mirror, c);
  s.mirror->baselines = false;
  if (!t.movers_named) name_movers(t, c);
  s.fighter = std::make_unique<bt::Fighter>();
  team_fighter_setup(*t.nav, *s.fighter, s.mirror->s, c.playernum, t.loaded->spec.entities.c_str(),
                     s.skill, s.traits, kHz, t.cfg.seed + 31u * static_cast<uint32_t>(s.index),
                     s.name);
  s.fighter->bot.yaw = c.set_angles[1];
  s.set_up = true;
  s.reset = false;
  s.said_ready = false;
  s.spawncount = c.spawncount;
  s.heard = c.prints.size();
  // The team's roster: every bot set up, in slot order.
  t.team.members.clear();
  for (auto& o : t.seats)
    if (o->set_up) {
      t.team.members.push_back(o->fighter.get());
      o->fighter->team = &t.team;
    }
  t.team.name = t.cfg.team;
  bt::team_init(t.team);
  if (t.nav->plan_made && !t.plan_said) {
    t.plan_said = true;
    std::string text = bt::plan_text(t.nav->plan, s.fighter->items);
    for (size_t at = 0; at < text.size();) {
      const size_t nl = text.find('\n', at);
      say(t, "plan: " + text.substr(at, nl - at));
      at = nl == std::string::npos ? text.size() : nl + 1;
    }
  }
  say(t, s.name + ": set up on " + t.map + " in slot " + std::to_string(c.playernum) + ", " +
             std::to_string(s.fighter->items.size()) + " items known of");
}

// The slots on the bot's own team, as its client's user infos say (people too).
uint32_t friends_of(const ClassicClientEnd& c) {
  uint32_t bits = 0;
  for (int i = 0; i < 32; i++)
    if (!c.userinfos[i].empty() && info_get(c.userinfos[i], "team") == c.cfg.team)
      bits |= 1u << static_cast<uint32_t>(i);
  return bits;
}

sh::MoveCmd brain(BotTeam& t, BotSeat& s) {
  ClassicClientEnd& c = s.c;
  if (!s.set_up || s.reset || s.spawncount != c.spawncount || !s.mirror || !s.fighter) return {};
  bt::Fighter& f = *s.fighter;
  mirror_update(*s.mirror, c);
  f.bot.senses.friends = friends_of(c);
  t.team.friends = f.bot.senses.friends;
  for (; s.heard < c.prints.size(); s.heard++) {
    std::string who, words;
    if (bt::split_team_line(c.prints[s.heard], who, words))
      bt::mind_hear(f, who, words, s.mirror->s.sv.time);
    else
      bt::mind_obituary(f, plain(c.prints[s.heard]), s.mirror->s.sv.time);
  }
  const sh::MoveCmd cmd = bt::fighter_command(f, s.mirror->s, c.playernum, bt::kPilotMsec);
  for (const std::string& line : f.outbox) client_command(c, "say_team \"" + line + "\"");
  f.outbox.clear();
  s.commands++;
  return cmd;
}

bool seat_command(BotTeam& t, BotSeat& s, const std::string& line) {
  pg::Words w;
  pg::tokenize_words(line.c_str(), w);
  const std::string cmd = pg::word(w, 0), arg = pg::word(w, 1);
  if (cmd.empty()) return true;
  std::string error;
  if (cmd == "skill") {
    sh::AimSkill k;
    if (!aim_skill_named(arg, k)) {
      say(t, s.name + ": no skill '" + arg + "'; next: expert, average, novice or perfect");
      return false;
    }
    s.skill = k;
    s.traits = sh::traits_of(k, sh::temper_of(s.traits));
    s.reset = true;
  } else if (cmd == "temper") {
    sh::Temper tp;
    if (!sh::temper_of(arg, tp)) {
      say(t, s.name + ": no temperament '" + arg + "'; next: aggressive, balanced or coward");
      return false;
    }
    s.traits = sh::traits_of(s.skill, tp);
    s.reset = true;
  } else if (cmd == "traits") {
    if (!sh::apply_traits(arg, s.traits, error)) {
      say(t, s.name + ": " + error);
      return false;
    }
    s.reset = true;
  } else if (cmd == "status") {
    say(t, s.name + ": " + sh::aim_skill_name(s.skill) + " " + sh::traits_text(s.traits));
    return true;
  } else {
    bool trait = false;
    for (size_t i = 0; i < sh::kTraits; i++)
      trait |= cmd == sh::trait_name(static_cast<sh::Trait>(i));
    if (trait) {
      if (!sh::apply_traits(cmd + "=" + arg, s.traits, error)) {
        say(t, s.name + ": " + error);
        return false;
      }
      s.reset = true;
    } else {
      // Anything else is the bot's console line to the server (`ready`, `break`, `say hi`).
      if (cmd == "ready" || cmd == "break") s.said_ready = true;
      client_command(s.c, line);
      return true;
    }
  }
  say(t, s.name + ": " + line + " (set up again on its next tick)");
  return true;
}

}  // namespace

bool bot_team_locs_from(BotTeamConfig& cfg, const std::string& folder) {
  auto files = std::make_shared<cl::SearchPath>();
  std::string why;
  if (folder.empty() || cfg.basedir.empty() || !cl::open_game_dir(cfg.basedir, folder, *files, why))
    return false;
  cfg.locs = [files](const std::string& map) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(*files, "locs/" + map + ".loc", store);
    return std::string(f.begin(), f.end());
  };
  return true;
}

bool aim_skill_named(std::string_view name, sh::AimSkill& out) {
  if (name == "expert" || name == "e")
    out = sh::AimSkill::expert;
  else if (name == "average" || name == "a")
    out = sh::AimSkill::average;
  else if (name == "novice" || name == "n")
    out = sh::AimSkill::novice;
  else if (name == "perfect" || name == "p")
    out = sh::AimSkill::perfect;
  else
    return false;
  return true;
}

bool bot_team_start(BotTeam& t, const BotTeamConfig& cfg, std::string& error) {
  t.cfg = cfg;
  std::string text = cfg.server;
  if (text.find(':') == std::string::npos) text += ":27500";
  if (cfg.address) {
    t.server = *cfg.address;
  } else if (!gn::parse_address(text, t.server)) {
    error = "bots: '" + cfg.server + "' is not host:port; next: 127.0.0.1:27500";
    return false;
  }
  if (cfg.bots < 1 || cfg.bots > 16) {
    error = "bots: " + std::to_string(cfg.bots) + " bots; next: 1 to 16";
    return false;
  }
  std::shared_ptr<Mount> mount = mount_of(t);
  mount->basedir = cfg.basedir;
  for (int k = 0; k < cfg.bots; k++) {
    auto s = std::make_unique<BotSeat>();
    s->index = k;
    s->name = cfg.name_prefix + std::to_string(k + 1);
    const std::string& sk = cfg.skills.empty()
                                ? std::string("average")
                                : cfg.skills[static_cast<size_t>(k) % cfg.skills.size()];
    if (!aim_skill_named(sk, s->skill)) {
      error = "bots: no skill '" + sk + "'; next: expert, average, novice or perfect";
      return false;
    }
    s->traits = sh::traits_of(s->skill, sh::Temper::balanced);
    if (!cfg.traits.empty() &&
        !sh::apply_traits(cfg.traits[static_cast<size_t>(k) % cfg.traits.size()], s->traits, error))
      return false;
    if (cfg.open ? !cfg.open(s->transport, k, error)
                 : !gn::open_udp_transport(s->transport, gn::ipv4(0, 0, 0, 0, 0), &error)) {
      std::string why = "bots: ";
      why += s->name;
      why += "'s socket did not open: ";
      why += error;
      error = std::move(why);
      return false;
    }
    ClientConfig cc;
    cc.name = s->name;
    cc.team = cfg.team;
    cc.topcolor = cc.bottomcolor = cfg.colour;
    cc.qport = static_cast<uint16_t>(cfg.first_qport + k);
    cc.seed = k + 1;
    cc.files = {"skins/base.pcx"};  // a bot needs no skin: never wait on its download
    if (cfg.read_file)
      cc.read_file = cfg.read_file;
    else
      cc.read_file = [mount](const std::string& gamedir, const std::string& path) {
        return read_from(*mount, gamedir, path);
      };
    BotSeat* sp = s.get();
    BotTeam* tp = &t;
    cc.brain = [tp, sp](int) { return brain(*tp, *sp); };
    client_start(s->c, s->transport, t.server, cc, 0);
    if (!cfg.out.empty()) {
      record_start(s->recording, false);
      s->c.record = &s->recording;
    }
    t.seats.push_back(std::move(s));
  }
  say(t, "bots: " + std::to_string(cfg.bots) + " joining " + text + " as team " + cfg.team);
  return true;
}

void bot_team_tick(BotTeam& t, int64_t now_us) {
  t.now_us = now_us;
  t.ticks++;
  for (auto& s : t.seats) client_tick(s->c, now_us);
  // The server's lines, once (the first bot's client hears everything said to all).
  if (!t.seats.empty()) {
    BotSeat& first = *t.seats[0];
    for (; first.printed < first.c.prints.size(); first.printed++) {
      std::string line = plain(first.c.prints[first.printed]);
      while (!line.empty() && line.back() == '\n') line.pop_back();
      say(t, line);
    }
  }
  for (auto& sp : t.seats) {
    BotSeat& s = *sp;
    const ClassicClientEnd& c = s.c;
    if (c.stage == ClientStage::dropped) {
      if (s.set_up || !s.reset) say(t, s.name + ": dropped: " + c.refused);
      s.set_up = false;
      s.reset = true;  // said once
      continue;
    }
    if (c.stage != ClientStage::active || c.playernum < 0 || !c.player_seen[c.playernum] ||
        c.models.size() < 2)
      continue;
    if (c.models[1] != t.map && !load_map(t, c, c.models[1])) continue;
    if (!t.nav) continue;
    if (!s.set_up || s.reset || s.spawncount != c.spawncount) {
      set_up(t, s);
      s.printed = s.c.prints.size();
      s.commands = 0;
      continue;
    }
    // A death: what it was doing, read back by cause after the match.
    const int health = c.stats[0];
    if (s.last_health > 0 && health <= 0 && s.fighter && s.mirror) {
      const bt::Fighter& f = *s.fighter;
      const sh::Edict& me = s.mirror->s.sv.edicts[static_cast<size_t>(c.playernum) + 1];
      const double tn = s.mirror->s.sv.time;
      std::string goal = "-";
      if (f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size())
        goal = bt::item_call(f.items[static_cast<size_t>(f.goal_item)]);
      std::string diving = "-";
      if (f.diving >= 0 && static_cast<size_t>(f.diving) < f.items.size())
        diving = bt::item_call(f.items[static_cast<size_t>(f.diving)]);
      s.deaths++;
      std::string line = "death " + s.name + " at ";
      line += f.locs ? sh::loc_name_at(*f.locs, me.origin) : "?";
      line += " (" + ge::core::num::to_fixed(me.origin.x, 0) + " ";
      line += ge::core::num::to_fixed(me.origin.y, 0) + " ";
      line += ge::core::num::to_fixed(me.origin.z, 0) + "): ";
      line += bt::fight_mode_name(f.mode);
      line += "/";
      line += bt::role_name(f.role);
      line += " goal " + goal + " water " + std::to_string(me.waterlevel) + " under ";
      line += f.under_since >= 0 ? ge::core::num::to_fixed(tn - f.under_since, 1) : "0";
      line += " s diving " + diving;
      if (f.swimming) line += " swimming-out";
      if (f.lost_to >= 0) line += " lost";
      say(t, line);
    }
    s.last_health = health;
    if (t.cfg.ready && !s.said_ready && s.commands * bt::kPilotMsec * 1000 >= kReadyAfterUs) {
      client_command(s.c, "ready");
      s.said_ready = true;
      say(t, s.name + ": ready");
    }
  }
}

void bot_team_command(BotTeam& t, const std::string& line_in) {
  std::string line = line_in;
  while (!line.empty() && (line[0] == '/' || line[0] == ' ')) line.erase(0, 1);
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
    line.pop_back();
  if (line.empty()) return;
  if (line == "report" || line == "status") {
    say(t, bot_team_report(t));
    return;
  }
  if (line.rfind("paths", 0) == 0) {
    t.cfg.paths = line.size() < 7 || line[6] != '0';
    say(t, std::string("bots: paths ") + (t.cfg.paths ? "drawn" : "hidden"));
    return;
  }
  if (line.rfind("autoready", 0) == 0) {
    t.cfg.ready = line.size() < 11 || line[10] != '0';
    say(t, std::string("bots: autoready ") + (t.cfg.ready ? "on" : "off"));
    return;
  }
  // `bot<N> <line>`: one bot's.
  if (line.rfind("bot", 0) == 0 && line.size() > 3 && line[3] >= '1' && line[3] <= '9') {
    size_t at = 3;
    int n = 0;
    while (at < line.size() && line[at] >= '0' && line[at] <= '9') n = n * 10 + (line[at++] - '0');
    if (at < line.size() && line[at] == ' ') {
      if (n < 1 || n > static_cast<int>(t.seats.size())) {
        say(t, "bots: no bot" + std::to_string(n) + "; next: bot1 to bot" +
                   std::to_string(t.seats.size()));
        return;
      }
      seat_command(t, *t.seats[static_cast<size_t>(n - 1)], line.substr(at + 1));
      return;
    }
  }
  for (auto& s : t.seats) seat_command(t, *s, line);
}

bool bot_team_stop(BotTeam& t, std::string& error) {
  for (auto& s : t.seats) client_command(s->c, "drop");
  for (int i = 0; i < 3; i++)
    for (auto& s : t.seats) client_tick(s->c, t.now_us + static_cast<int64_t>(i + 1) * 13000);
  if (t.cfg.out.empty()) return true;
  for (auto& s : t.seats) {
    record_stop(s->recording);
    const std::string path = t.cfg.out + "/" + s->name + ".demo";
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(s->recording.bytes.data()),
              static_cast<std::streamsize>(s->recording.bytes.size()));
    if (!out) {
      error = "bots: could not write " + path + "; next: make the folder " + t.cfg.out;
      return false;
    }
  }
  std::ofstream team(t.cfg.out + "/team.txt");
  for (const bt::TeamLine& l : t.team.log)
    team << ge::core::num::to_fixed(l.t, 1) << " " << l.slot << " " << l.text << "\n";
  std::ofstream log(t.cfg.out + "/log.txt");
  for (const std::string& l : t.log) log << l << "\n";
  return true;
}

std::string bot_team_report(const BotTeam& t) {
  std::string r;
  int ours = 0, theirs = 0;
  if (!t.seats.empty()) {
    const ClassicClientEnd& c = t.seats[0]->c;
    for (int i = 0; i < 32; i++) {
      if (c.userinfos[i].empty() || !info_get(c.userinfos[i], "*spectator").empty()) continue;
      (info_get(c.userinfos[i], "team") == c.cfg.team ? ours : theirs) += c.frags[i];
    }
  }
  r += "team " + t.cfg.team + " on " + (t.map.empty() ? std::string("no map yet") : t.map) +
       ": frags " + std::to_string(ours) + " to " + std::to_string(theirs) + "\n";
  for (const auto& sp : t.seats) {
    const BotSeat& s = *sp;
    const ClassicClientEnd& c = s.c;
    r += "  " + s.name + " (" + sh::aim_skill_name(s.skill) + ") ";
    if (!s.set_up || !s.fighter) {
      r += std::string(client_stage_name(c.stage)) +
           (c.refused.empty() ? std::string() : ": " + c.refused) + "\n";
      continue;
    }
    const bt::Fighter& f = *s.fighter;
    const sh::Vec3f at = s.mirror->s.sv.edicts[static_cast<size_t>(c.playernum) + 1].origin;
    std::string goal = "-";
    if (f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size())
      goal = bt::item_call(f.items[static_cast<size_t>(f.goal_item)]);
    r += "h" + std::to_string(c.stats[0]) + " a" + std::to_string(c.stats[4]) + " " +
         bt::fight_mode_name(f.mode) + "/" + bt::role_name(f.role) + " goal " + goal + " at " +
         (f.locs ? sh::loc_name_at(*f.locs, at) : std::string("?")) + " frags " +
         std::to_string(c.frags[c.playernum]) + "\n";
  }
  return r;
}

}  // namespace shooters_edge::net

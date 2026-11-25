// QuakeWorld's captain mode, defined: the rules in qw_captains.hpp's header.
//
// ge:summary QuakeWorld's captain mode: the election's words, the picks, the lines and the refusals
// ge:keywords qw-captains captain-mode qw_captains_line qw_captains_impulse team-picking
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_captains_test.cpp
#include "shooters_edge/net/qw_captains.hpp"

#include <cstdlib>
#include <string>
#include <vector>

#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/qw_server.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
constexpr int kSeats = ge::shooter::kMaxClients;
constexpr const char* kRole = "captain";

const gs::ElectionWords& words() {
  static const gs::ElectionWords w = [] {
    gs::ElectionWords x;
    x.requested = "{name} has requested captain status!";
    x.approve = "Type yes in console to approve";
    x.abort_hint = "Type captain to abort election";
    x.gives = "{name} gives his vote";
    x.more = "{n} more {votes} needed";
    x.still_good = "--- your vote is still good ---";
    x.not_self = "You cannot vote for yourself";
    x.withdraws = "{name} withdraws his vote";
    x.timed_out = "The voting has timed out.\nElection aborted";
    x.aborted = "{name} aborts election!";
    x.busy = "An election is already in progress";
    x.wait = "Wait {n} {seconds}!";
    return x;
  }();
  return w;
}

void tell(QwServer& s, int to, const std::string& line) {
  if (to == -1)
    qw_print_all(s, line + "\n");
  else if (to >= 0 && to < kSeats)
    client_print(s, to, 2, line + "\n");
}

// The election's lines said.
void flush(const QwCaptainsGame& g) {
  gs::Election& e = *g.election;
  for (size_t i = 0; i < e.said.size(); i++) tell(*g.qw, e.to[i], e.said[i]);
  e.said.clear();
  e.to.clear();
}

bool seated(const QwServer& s, int slot) {
  const QwClient& c = s.clients[slot];
  return c.state != ConnState::free && !c.spectator && info_get(c.userinfo, "*bot").empty();
}

// The voters: the people playing (no spectators, no bots).
uint32_t voters(const QwServer& s) {
  uint32_t v = 0;
  for (int i = 0; i < kSeats; i++)
    if (seated(s, i)) v |= 1u << i;
  return v;
}

int players(const QwServer& s) {
  int n = 0;
  for (int i = 0; i < kSeats; i++) n += seated(s, i) ? 1 : 0;
  return n;
}

std::string info(const QwServer& s, int slot, const char* key) {
  return info_get(s.clients[slot].userinfo, key);
}

int captains(const QwCaptains& c) { return (c.captain[0] >= 0 ? 1 : 0) + (c.captain[1] >= 0); }
int captain_of(const QwCaptains& c, int slot) {
  return slot < 0 ? -1 : c.captain[0] == slot ? 0 : c.captain[1] == slot ? 1 : -1;
}

void present(QwCaptains& c, QwServer& s) {
  const int n = captains(c);
  tell(s, -1, n == 0 ? "No captain present" : std::to_string(n) + " captain present");
}

// A player's team and colours set, on the server and in his own user info.
void set_team(QwServer& s, int slot, const std::string& team, const std::string& top,
              const std::string& bottom) {
  qw_set_info(s, slot, "team", team);
  qw_set_info(s, slot, "topcolor", top);
  qw_set_info(s, slot, "bottomcolor", bottom);
  stuff(s, slot,
        "setinfo team \"" + team + "\"\nsetinfo topcolor " + top + "\nsetinfo bottomcolor " +
            bottom + "\n");
}

void clear_numbers(QwServer& s) {
  for (QwClient& cl : s.clients) cl.pick_number = 0;
}

void end_picking(QwCaptains& c, QwServer& s) {
  tell(s, -1, "All players chosen. Captain modes exited");
  for (const int cap : c.captain)
    if (cap >= 0) tell(s, cap, "You are no longer a captain");
  c.captain = {-1, -1};
  c.fire_told = 0;
  clear_numbers(s);
}

void say_picking(QwCaptains& c, QwServer& s) {
  const int turn = gs::draft_turn(c.draft);
  if (turn >= 0) tell(s, -1, s.clients[c.captain[static_cast<size_t>(turn)]].name + " is picking");
}

// The lobby's own team for a slot (sv_lobby: the lobby then shows it as the team's look).
void lobby_side(const QwCaptainsGame& g, int slot, gs::Team team) {
  if (g.lobby != nullptr && slot >= 0 && slot < gs::kLobbySeats)
    gs::lobby_team(*g.lobby, slot, team, g.now);
}

void apply_picks(QwCaptains& c, const QwCaptainsGame& g, const std::vector<gs::DraftPick>& picks) {
  QwServer& s = *g.qw;
  for (const gs::DraftPick& p : picks) {
    const auto k = static_cast<size_t>(p.captain);
    s.clients[p.slot].pick_number = 0;
    set_team(s, p.slot, c.team[k], c.top[k], c.bottom[k]);
    if (g.lobby != nullptr)
      lobby_side(g, p.slot, g.lobby->seats[static_cast<size_t>(c.captain[k])].team);
    tell(s, -1, s.clients[p.slot].name + " set to team " + c.team[k]);
    tell(s, p.slot, "You were picked by " + s.clients[c.captain[k]].name);
    tell(s, p.slot, "Time to go ready");
  }
}

void begin_picking(QwCaptains& c, const QwCaptainsGame& g) {
  QwServer& s = *g.qw;
  if (players(s) < 3) {
    tell(s, -1, "Not enough players present");
    tell(s, -1, "Captain modes exited");
    c.captain = {-1, -1};
    return;
  }
  tell(s, -1, "Both captains elected");
  tell(s, -1, "Team picking begins");
  std::vector<int> pool;
  for (int k = 0; k < 2; k++) {
    const int cap = c.captain[static_cast<size_t>(k)];
    c.team[static_cast<size_t>(k)] = info(s, cap, "team");
    c.top[static_cast<size_t>(k)] = info(s, cap, "topcolor");
    c.bottom[static_cast<size_t>(k)] = info(s, cap, "bottomcolor");
  }
  for (int i = 0; i < kSeats; i++)
    if (seated(s, i) && captain_of(c, i) < 0) pool.push_back(i);
  gs::draft_start(c.draft, c.captain[0], c.captain[1], pool, g.seed);
  for (const int i : pool) {
    s.clients[i].pick_number = c.draft.number[static_cast<size_t>(i)];
    set_team(s, i, "", "0", "0");
    qw_set_info(s, i, "skin", "");
    if (g.lobby != nullptr && i < gs::kLobbySeats) gs::lobby_ready(*g.lobby, i, false, g.now);
    lobby_side(g, i, gs::Team::none);
  }
  for (const int cap : c.captain) tell(s, cap, "Use numbers or impulses to choose");
  say_picking(c, s);
}

void taken_in(QwCaptains& c, const QwCaptainsGame& g, int slot) {
  QwServer& s = *g.qw;
  const size_t k = c.captain[0] < 0 ? 0 : 1;
  c.captain[k] = slot;
  tell(s, -1, s.clients[slot].name + " becomes a captain");
  if (captains(c) == 1)
    tell(s, -1, "One more captain should be elected");
  else
    begin_picking(c, g);
}

// A captain gone (resigned or left): the picking, if any, called off.
void captain_gone(QwCaptains& c, const QwCaptainsGame& g, int slot) {
  QwServer& s = *g.qw;
  c.captain[static_cast<size_t>(captain_of(c, slot))] = -1;
  if (c.draft.active) {
    gs::draft_leave(c.draft, slot);
    tell(s, -1, "Player picking aborted");
    clear_numbers(s);
    c.fire_told = 0;
  }
  present(c, s);
}

void ask(QwCaptains& c, const QwCaptainsGame& g, int slot) {
  QwServer& s = *g.qw;
  gs::Election& e = *g.election;
  const std::string& name = s.clients[slot].name;
  if (captain_of(c, slot) >= 0) {
    tell(s, -1, name + " is no longer a captain");
    captain_gone(c, g, slot);
    return;
  }
  if (gs::election_running(e) && e.open.slot == slot && e.open.name == kRole) {
    gs::election_abort(e, words(), slot, name, g.now);
    return;
  }
  if (g.in_match || c.draft.active) return;
  if (!g.team_mode) return tell(s, slot, "No team picking in non team mode");
  if (!seated(s, slot)) return;
  if (players(s) < 3) return tell(s, slot, "Not enough players present");
  if (captains(c) >= 2) return tell(s, slot, "Only 2 captains are allowed");
  if (gs::election_running(e)) return tell(s, slot, words().busy);
  const std::string team = info(s, slot, "team");
  if (team.empty()) return tell(s, slot, "Set your team name first");
  for (const int other : c.captain) {
    if (other < 0) continue;
    if (info(s, other, "team") == team) {
      tell(s, slot, "A captain with team " + team + " already exists");
      return tell(s, slot, "Choose a new team name");
    }
    if (info(s, other, "topcolor") == info(s, slot, "topcolor") &&
        info(s, other, "bottomcolor") == info(s, slot, "bottomcolor")) {
      tell(s, slot, "A captain with your color already exists");
      return tell(s, slot, "Change to new color");
    }
  }
  e.settings.percent = 51;
  if (g.settings != nullptr) {
    const auto it = g.settings->rules.find("k_vp_captain");
    if (it != g.settings->rules.end()) e.settings.percent = std::atoi(it->second.c_str());
  }
  gs::election_open(e, words(), {kRole, -1, slot}, name, voters(s), g.now);
}

}  // namespace

bool qw_captains_picking(const QwCaptains& c) { return c.draft.active; }

bool qw_captains_line(QwCaptains& c, const QwCaptainsGame& g, int slot, const std::string& line) {
  if (g.qw == nullptr || g.election == nullptr || slot < 0 || slot >= kSeats) return false;
  QwServer& s = *g.qw;
  gs::Election& e = *g.election;
  bool taken = false;
  if (line == "captain") {
    ask(c, g, slot);
    taken = true;
  } else if ((line == "yes" || line == "no") && gs::election_running(e) && e.open.name == kRole) {
    taken =
        gs::election_vote(e, words(), slot, s.clients[slot].name, line == "yes", voters(s), g.now);
  }
  flush(g);
  if (e.passed.slot >= 0 && e.passed.name == kRole) {
    const int who = e.passed.slot;
    e.passed = {};
    taken_in(c, g, who);
  }
  return taken;
}

uint8_t qw_captains_impulse(QwCaptains& c, const QwCaptainsGame& g, int slot, uint8_t impulse,
                            bool fresh) {
  if (g.qw == nullptr || !c.draft.active) return impulse;
  if (!fresh || captain_of(c, slot) < 0) return 0;
  QwServer& s = *g.qw;
  std::vector<gs::DraftPick> picks;
  switch (gs::draft_pick(c.draft, slot, impulse, picks)) {
    case gs::DraftRefusal::not_your_turn:
      tell(s, slot, "It's not your turn");
      break;
    case gs::DraftRefusal::no_such_player:
      tell(s, slot, "No such player. Pick another one");
      break;
    case gs::DraftRefusal::none:
      apply_picks(c, g, picks);
      if (c.draft.active)
        say_picking(c, s);
      else
        end_picking(c, s);
      break;
    case gs::DraftRefusal::not_captain:
      break;
  }
  return 0;
}

uint8_t qw_captains_buttons(QwCaptains& c, const QwCaptainsGame& g, int slot, uint8_t buttons) {
  if (g.qw == nullptr || !c.draft.active || (buttons & 1u) == 0) return buttons;
  if (slot >= 0 && slot < kSeats && ((c.fire_told >> slot) & 1u) == 0) {
    c.fire_told |= 1u << slot;
    tell(*g.qw, slot, "Nobody fires while the captains pick");
  }
  return static_cast<uint8_t>(buttons & ~1u);
}

bool qw_captains_info(QwCaptains& c, const QwCaptainsGame& g, int slot, const std::string& key,
                      const std::string& value) {
  if (g.qw == nullptr || key != "team" || slot < 0 || slot >= kSeats) return true;
  QwServer& s = *g.qw;
  const std::string now = info(s, slot, "team");
  if (value == now) return true;
  auto refuse = [&](const char* why) {
    tell(s, slot, why);
    stuff(s, slot, "setinfo team \"" + now + "\"\n");
    return false;
  };
  if (g.in_match) return refuse("You may not change team during game");
  const bool ready = g.lobby != nullptr && slot < gs::kLobbySeats &&
                     g.lobby->seats[static_cast<size_t>(slot)].ready;
  if (value.empty() && g.team_mode && ready) return refuse("Empty team not allowed");
  const gs::Election& e = *g.election;
  const bool candidate = gs::election_running(e) && e.open.slot == slot && e.open.name == kRole;
  if (captain_of(c, slot) < 0 && !candidate && !c.draft.active) return true;
  return refuse("You may not change team");
}

namespace {

void join(QwCaptains& c, const QwCaptainsGame& g, int slot) {
  if (g.qw == nullptr || !c.draft.active || !seated(*g.qw, slot)) return;
  const int n = gs::draft_join(c.draft, slot);
  if (n == 0) return;
  g.qw->clients[slot].pick_number = n;
  set_team(*g.qw, slot, "", "0", "0");
}

void leave(QwCaptains& c, const QwCaptainsGame& g, int slot) {
  if (g.qw == nullptr || slot < 0 || slot >= kSeats) return;
  QwServer& s = *g.qw;
  s.clients[slot].pick_number = 0;
  if (g.election != nullptr) {
    gs::election_leave(*g.election, words(), slot, voters(s) & ~(1u << slot), g.now);
    flush(g);
  }
  if (captain_of(c, slot) >= 0) {
    tell(s, -1, "A captain has left");
    captain_gone(c, g, slot);
    return;
  }
  if (!c.draft.active) return;
  gs::draft_leave(c.draft, slot);
  if (!c.draft.active) end_picking(c, s);
}

}  // namespace

void qw_captains_frame(QwCaptains& c, const QwCaptainsGame& g) {
  if (g.qw == nullptr || g.election == nullptr) return;
  const uint32_t now = voters(*g.qw);
  for (int i = 0; i < kSeats; i++) {
    const uint32_t bit = 1u << i;
    if ((c.seen & bit) != 0 && (now & bit) == 0) leave(c, g, i);
    if ((c.seen & bit) == 0 && (now & bit) != 0) join(c, g, i);
  }
  c.seen = now;
  gs::election_tick(*g.election, words(), g.now);
  flush(g);
  if (g.election->passed.slot >= 0 && g.election->passed.name == kRole) {
    const int who = g.election->passed.slot;
    g.election->passed = {};
    taken_in(c, g, who);
  }
}

}  // namespace shooters_edge::net

// The watched player's record in a multiview demo (mvd_status.hpp).
//
// ge:summary The watched player's took, lost and kills record in a multiview demo (mvd_status)
// ge:keywords mvd_status_frame mvd_status_lines
// ge:skill shooter replay ui
// ge:test tests/shooters_edge_mvd_info_test.cpp
#include "shooters_edge/client/mvd_status.hpp"

#include <algorithm>
#include <cstdio>

namespace shooters_edge::client {

namespace {

constexpr int kSlots = 32;
// The items' bits.
constexpr int kSg = 1, kSsg = 2, kNg = 4, kSng = 8, kGl = 16, kRl = 32, kLg = 64, kAxe = 4096;
constexpr int kGa = 8192, kYa = 16384, kRa = 32768;
constexpr int kRing = 524288, kPent = 1048576, kQuad = 4194304;
// The weapons in the kills' order, their bits and their words.
constexpr std::array<int, kStatusWeapons> kOrderBits{kRl, kLg, kGl, kSng, kNg, kSsg, kSg, kAxe};
constexpr std::array<const char*, kStatusWeapons> kOrderWords{
    "tp_name_rl", "tp_name_lg",  "tp_name_gl", "tp_name_sng",
    "tp_name_ng", "tp_name_ssg", "tp_name_sg", "tp_name_axe"};
constexpr std::array<const char*, kStatusWeapons> kOrderLabels{"RL", "LG",  "GL", "SNG",
                                                               "NG", "SSG", "SG", "AXE"};

int order_of(int weapon_bit) {
  for (size_t k = 0; k < kOrderBits.size(); k++)
    if (kOrderBits[k] == weapon_bit) return static_cast<int>(k);
  return -1;
}

bool gained(int was, int now, int bit) { return (now & bit) != 0 && (was & bit) == 0; }

std::string fixed3(double v, int width) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%*.3f", width, v);
  return buf;
}

std::string padded(int v, int width) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%*d", width, v);
  return buf;
}

std::string word(const ge::console::Console& con, const char* name) {
  const ge::console::Cvar* v = con.cvars().find(name);
  return v != nullptr ? v->value : std::string();
}

void run_rows(std::vector<MvdStatusLine>& out, const std::vector<MvdLife>& runs, double now,
              bool skip_empty) {
  const size_t from = runs.size() > 4 ? runs.size() - 4 : 0;
  for (size_t i = from; i < runs.size(); i++) {
    const MvdLife& r = runs[i];
    const double t = (r.to < 0 ? now : r.to) - r.from;
    if (skip_empty && !(t > 0)) continue;
    out.push_back({padded(static_cast<int>(i + 1), 3) + " " + fixed3(t, 9) + " " +
                   padded(r.frags, 5) + " " + padded(r.tks, 3)});
  }
}

}  // namespace

void mvd_status_frame(MvdStatus& m, const Demo& d) {
  const DemoState& s = d.state;
  if (s.format != DemoFormat::multiview || s.map.empty()) return;
  if (s.levels != m.levels || s.time < m.time) {
    m = MvdStatus{};
    m.levels = s.levels;
  }
  m.time = s.time;
  for (int i = 0; i < kSlots; i++) {
    const size_t k = static_cast<size_t>(i);
    MvdPlayerRecord& p = m.players[k];
    if (s.names[k].empty() || s.spectator[k] || k >= d.player_stats.size()) {
      p.seen = false;
      continue;
    }
    const auto& st = d.player_stats[k];
    const int items = st[kStatItems], armour = st[kStatArmour], health = st[kStatHealth];
    const int frags = s.frags[k];
    const int in_hand = order_of(st[kStatActiveWeapon]);
    if (!p.seen) {  // the first frame: what he has is not counted as taken
      p.seen = true;
      p.items = items;
      p.armour = armour;
      p.health = health;
      p.frags = frags;
      p.flashed = s.flashed[k];
      if (health > 0 && p.lives.empty()) p.lives.push_back({s.time, -1, 0, 0});
      continue;
    }
    // Took.
    const std::array<int, 3> weapons{kRl, kLg, kGl};
    for (size_t w = 0; w < weapons.size(); w++)
      if (gained(p.items, items, weapons[w])) p.took[w]++;
    const std::array<int, 3> armours{kRa, kYa, kGa};
    for (size_t a = 0; a < armours.size(); a++)
      if ((items & armours[a]) != 0 && (gained(p.items, items, armours[a]) || armour > p.armour))
        p.took[3 + a]++;
    if (gained(p.items, items, kRing)) p.took[6]++;
    if (gained(p.items, items, kQuad)) {
      p.took[7]++;
      p.quad_runs.push_back({s.time, -1, 0, 0});
    }
    if (gained(p.items, items, kPent)) p.took[8]++;
    if (health > p.health && health > 100 && p.health > 0) p.took[9]++;
    if ((items & kQuad) == 0 && !p.quad_runs.empty() && p.quad_runs.back().to < 0)
      p.quad_runs.back().to = s.time;
    // The gun's flash: the last fired weapon.
    if (s.flashed[k] != p.flashed && in_hand >= 0) p.last_fired = in_hand;
    p.flashed = s.flashed[k];
    // Frags: kills and team kills by the last fired weapon (SPAWN with none).
    const int slot = p.last_fired >= 0 ? p.last_fired : kStatusWeapons;
    MvdLife* life = !p.lives.empty() && p.lives.back().to < 0 ? &p.lives.back() : nullptr;
    MvdLife* quad =
        !p.quad_runs.empty() && p.quad_runs.back().to < 0 ? &p.quad_runs.back() : nullptr;
    if (frags > p.frags) {
      p.kills[static_cast<size_t>(slot)] += frags - p.frags;
      if (life != nullptr) life->frags++;
      if (quad != nullptr) quad->frags++;
    } else if (frags < p.frags) {
      p.teamkills[static_cast<size_t>(slot)] += p.frags - frags;
      if (life != nullptr) life->tks++;
      if (quad != nullptr) quad->tks++;
    }
    // Death and spawn.
    if (p.health > 0 && health <= 0) {
      p.deaths++;
      if (life != nullptr) life->to = s.time;
      const std::array<int, 3> lost{kRl, kLg, kGl};
      for (size_t w = 0; w < lost.size(); w++)
        if (st[kStatActiveWeapon] == lost[w]) p.lost[w]++;
      if ((p.items & kQuad) != 0) p.lost[3]++;
      p.last_fired = -1;
    } else if (p.health <= 0 && health > 0) {
      p.lives.push_back({s.time, -1, 0, 0});
      p.last_fired = -1;
    }
    p.items = items;
    p.armour = armour;
    p.health = health;
    p.frags = frags;
  }
}

std::vector<MvdStatusLine> mvd_status_lines(const MvdStatus& m, const Demo& d,
                                            const ge::console::Console& con) {
  const DemoState& s = d.state;
  std::vector<MvdStatusLine> out;
  if (s.format != DemoFormat::multiview || s.intermission) return out;
  const auto listed = [&s](int i) {
    const size_t k = static_cast<size_t>(i);
    return i >= 0 && i < kSlots && !s.names[k].empty() && !s.spectator[k];
  };
  int who = listed(s.player) ? s.player : -1;
  for (int i = 0; i < kSlots && who < 0; i++)
    if (listed(i)) who = i;
  static const MvdPlayerRecord kNone;
  const MvdPlayerRecord& p = who >= 0 ? m.players[static_cast<size_t>(who)] : kNone;
  const auto n = [](int v) { return std::to_string(v); };
  out.push_back({who >= 0 ? s.names[static_cast<size_t>(who)] : ""});
  out.push_back({"Took", true});
  out.push_back({"RL: " + n(p.took[0]) + " LG: " + n(p.took[1]) + " GL: " + n(p.took[2]) +
                 " RA: " + n(p.took[3]) + " YA: " + n(p.took[4]) + " GA:" + n(p.took[5])});
  out.push_back({"Ring: " + n(p.took[6]) + " Quad: " + n(p.took[7]) + " Pent: " + n(p.took[8]) +
                 " MH: " + n(p.took[9])});
  out.push_back({"Deaths: " + n(p.deaths)});
  out.push_back({"Average Run:"});
  out.push_back({"Time      Frags TKS"});
  double time = 0, frags = 0, tks = 0;
  for (const MvdLife& l : p.lives) {
    time += (l.to < 0 ? m.time : l.to) - l.from;
    frags += l.frags;
    tks += l.tks;
  }
  const double lives = p.lives.empty() ? 1 : static_cast<double>(p.lives.size());
  out.push_back(
      {fixed3(time / lives, 9) + " " + fixed3(frags / lives, 0) + " " + fixed3(tks / lives, 0)});
  out.push_back({"Last 3 Runs:"});
  out.push_back({"No. Time      Frags TKS"});
  run_rows(out, p.lives, m.time, false);
  out.push_back({"Last Fired Weapon: " +
                 word(con, p.last_fired >= 0 ? kOrderWords[static_cast<size_t>(p.last_fired)]
                                             : "tp_name_none")});
  out.push_back({"Lost", true});
  out.push_back({"RL: " + n(p.lost[0]) + " LG: " + n(p.lost[1]) + " GL: " + n(p.lost[2]) +
                 " QUAD: " + n(p.lost[3])});
  const auto by_weapon = [&](const std::array<int, kStatusWeapons + 1>& c) {
    std::string line;
    for (size_t w = 0; w < kStatusWeapons; w++) {
      if (w > 0) line += ' ';
      line += std::string(kOrderLabels[w]) + ": " + n(c[w]);
    }
    out.push_back({line});
    out.push_back({"SPAWN: " + n(c[kStatusWeapons])});
  };
  out.push_back({"Kills", true});
  by_weapon(p.kills);
  out.push_back({"Teamkills", true});
  by_weapon(p.teamkills);
  out.push_back({"Last 3 Quad Runs:"});
  out.push_back({"No. Time      Frags TKS"});
  run_rows(out, p.quad_runs, m.time, true);
  return out;
}

}  // namespace shooters_edge::client

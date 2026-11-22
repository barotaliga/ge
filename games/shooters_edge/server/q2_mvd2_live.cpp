// The definitions behind q2_mvd2_live.hpp.
//
// ge:summary Quake II's multiview frames from a live server: settings, players, entities, casts
// ge:keywords mvd2 live-recorder capture unicast multicast
// ge:skill shooter replay
// ge:test tests/shooters_edge_q2_mvd_server_test.cpp
#include "shooters_edge/server/q2_mvd2_live.hpp"

#include <algorithm>
#include <cstring>

namespace shooters_edge::server {

namespace {

constexpr uint8_t kPrint = 10, kStuffText = 11, kCenterPrint = 15, kLayout = 4;
constexpr int kSpectatorMove = 1;

void put_str(std::vector<uint8_t>& b, const std::string& s) {
  b.insert(b.end(), s.begin(), s.end());
  b.push_back(0);
}

bool captured(const RulesCommon& c, int slot, const Q2PlayerState& p) {
  return c.clients[slot].active && !c.clients[slot].spectator && p.type != kSpectatorMove &&
         p.fov != 0;
}

void players(const RulesCommon& c, const Mvd2Capture& cap, Mvd2Frame& out) {
  out.players.assign(sh::kMaxClients, Mvd2Player{});
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2PlayerState p = q2_player_state(c, i, cap.moves[i]);
    if (!captured(c, i, p)) continue;
    if (cap.no_blend) std::memset(p.blend, 0, sizeof p.blend);
    if (cap.no_gun) {
      p.gun_index = 0;
      p.gun_frame = 0;
    }
    out.players[size_t(i)].in = true;
    out.players[size_t(i)].s = p;
  }
}

void entities(const RulesCommon& c, Mvd2Frame& out) {
  out.entities.assign(kQ2Entities, Q2EntityState{});
  for (int n = 1; n < kQ2Entities; n++) out.entities[size_t(n)] = q2_entity_state(c, n);
}

// The one slot `to` names, or -1 when it names none or more than one.
int one_slot(uint32_t to) {
  if (to == 0 || (to & (to - 1)) != 0) return -1;
  int slot = 0;
  while ((to >> slot) != 1u) slot++;
  return slot;
}

}  // namespace

void q2_mvd2_level(const RulesCommon& c, const Q2WireOptions& opt, const Mvd2Capture& cap,
                   Mvd2Live& live, Mvd2Level& out) {
  out = Mvd2Level{};
  out.server_count = opt.server_count;
  out.observer = -1;
  q2_config_strings(c, opt, out.config_strings);
  out.config_strings.resize(kQ2ConfigStrings);
  players(c, cap, out.base);
  entities(c, out.base);
  live = Mvd2Live{};
  live.sent = out.config_strings;
  live.events_done = c.out.events_total;
  for (int i = 0; i < sh::kMaxClients; i++) live.layouts[i] = c.world.clients[i].layout;
}

void q2_mvd2_frame(const RulesCommon& c, const Q2WireOptions& opt, const Mvd2Capture& cap,
                   Mvd2Live& live, Mvd2Frame& out) {
  out = Mvd2Frame{};
  std::vector<std::string> now;
  q2_config_strings(c, opt, now);
  now.resize(kQ2ConfigStrings);
  live.sent.resize(kQ2ConfigStrings);
  for (int i = 0; i < kQ2ConfigStrings; i++)
    if (now[size_t(i)] != live.sent[size_t(i)]) {
      out.config_strings.emplace_back(i, now[size_t(i)]);
      live.sent[size_t(i)] = now[size_t(i)];
    }
  players(c, cap, out);
  entities(c, out);
  // Every slot that is in the game: a cast to all of them is to everyone.
  uint32_t everyone = 0;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (c.clients[i].active) everyone |= 1u << i;
  const auto in = [&out](int slot) { return slot >= 0 && out.players[size_t(slot)].in; };
  const auto unicast = [&out](int slot, std::vector<uint8_t> bytes) {
    if (bytes.size() > kMvd2MostCast) return;  // too big for the stream: left out
    out.unicasts.emplace_back(slot, std::move(bytes));
  };
  // A captured player's layout when it changed.
  for (int i = 0; i < sh::kMaxClients; i++) {
    const std::string& layout = c.world.clients[i].layout;
    if (layout == live.layouts[i]) continue;
    live.layouts[i] = layout;
    if (!in(i)) continue;
    std::vector<uint8_t> b{kLayout};
    put_str(b, layout);
    unicast(i, std::move(b));
  }
  const std::vector<NetEvent>& events = c.out.events;
  const int64_t first = c.out.events_total - static_cast<int64_t>(events.size());
  const size_t from = static_cast<size_t>(std::max<int64_t>(0, live.events_done - first));
  for (size_t k = from; k < events.size(); k++) {
    const NetEvent& e = events[k];
    const bool all = everyone != 0 && (e.to & everyone) == everyone;
    const int slot = all ? -1 : one_slot(e.to & everyone);
    if (e.kind == EventKind::print) {
      if (all) {
        out.prints.emplace_back(std::clamp(e.level, 0, 3), e.text);
      } else if (in(slot)) {
        std::vector<uint8_t> b{kPrint, static_cast<uint8_t>(std::clamp(e.level, 0, 3))};
        put_str(b, e.text);
        unicast(slot, std::move(b));
      }
    } else if (e.kind == EventKind::centerprint) {
      std::vector<uint8_t> b{kCenterPrint};
      put_str(b, e.text);
      if (all) {
        for (int i = 0; i < sh::kMaxClients; i++)
          if (in(i)) unicast(i, b);
      } else if (in(slot)) {
        unicast(slot, std::move(b));
      }
    } else if (e.kind == EventKind::stuffcmd) {
      if (!in(slot) || e.text.rfind("play ", 0) != 0) continue;  // only a sound's line
      std::vector<uint8_t> b{kStuffText};
      put_str(b, e.text);
      unicast(slot, std::move(b));
    } else if (e.kind == EventKind::point_effect || e.kind == EventKind::trail_effect) {
      std::vector<uint8_t> b;
      if (q2_write_effect(b, e) && b.size() <= kMvd2MostCast)
        out.multicasts.push_back(std::move(b));
    } else if (e.kind == EventKind::sound) {
      const auto at = std::find(c.sounds.begin(), c.sounds.end(), e.text);
      if (at == c.sounds.end() || at == c.sounds.begin()) continue;
      Mvd2Sound s;
      s.index = static_cast<int>(at - c.sounds.begin());
      s.volume = e.values[0] > 0 ? std::clamp(static_cast<int>(e.values[0]), 0, 255) : 255;
      s.attenuation = std::clamp(static_cast<int>(e.values[1] * 64.0f), 0, 255);
      s.entity = std::clamp(e.ent, 0, kQ2Entities - 1);
      s.channel = e.level & 7;
      out.sounds.push_back(s);
    }
  }
  live.events_done = c.out.events_total;
}

}  // namespace shooters_edge::server

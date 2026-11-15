// The sequel's game broadcast and watch hooks, defined (sequel_game_tv.hpp).
//
// ge:summary The sequel's game broadcast and watch hooks, defined
// ge:keywords tv-frame watch-hooks sequel player-view sequel_tv_state
// ge:skill net replay shooter
// ge:test tests/shooters_edge_sequel_record_server_test.cpp
#include "shooters_edge/net/sequel_game_tv.hpp"

#include <array>
#include <cmath>
#include <span>
#include <string>

namespace shooters_edge::net {

namespace {

namespace sv = shooters_edge::server;

void put16(std::vector<uint8_t>& out, int32_t v) {
  const auto u = static_cast<uint16_t>(v);
  out.push_back(static_cast<uint8_t>(u & 0xFF));
  out.push_back(static_cast<uint8_t>(u >> 8));
}

// Eighths of a unit, and 65536ths of a turn, as shorts (wrapping as the wire's do).
int32_t eighths(float v) { return static_cast<int32_t>(std::lround(static_cast<double>(v) * 8)); }
int32_t turn16(float deg) {
  return static_cast<int32_t>(std::lround(static_cast<double>(deg) * 65536.0 / 360.0));
}

// The bytes of one view from the front of `bytes`; 0 when it does not read.
constexpr size_t kViewBytes = 1 + 6 + 6 + 1 + 2 + 1 + 2 + 1 + 2 + 2 + 1 + 1 + 1 + 64;
size_t view_size(std::span<const uint8_t> bytes) {
  if (bytes.empty()) return 0;
  if (bytes[0] == 0) return 1;
  return bytes[0] == 1 && bytes.size() >= kViewBytes ? kViewBytes : 0;
}

struct Watch {
  std::array<std::vector<uint8_t>, kSequelViewSlots> views;
};

bool keep(Watch& w, std::span<const uint8_t> bytes, std::string& error) {
  size_t at = 0;
  for (int k = 0; k < kSequelViewSlots; k++) {
    const size_t n = view_size(bytes.subspan(at));
    if (n == 0) {
      error = "the sequel's frame does not read at slot " + std::to_string(k) + " (byte " +
              std::to_string(at) + " of " + std::to_string(bytes.size()) + ")";
      return false;
    }
    w.views[static_cast<size_t>(k)].assign(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                                           bytes.begin() + static_cast<std::ptrdiff_t>(at + n));
    at += n;
  }
  if (at != bytes.size()) {
    error = "the sequel's frame has " + std::to_string(bytes.size() - at) + " bytes past its slots";
    return false;
  }
  return true;
}

void* watch_open() { return new Watch(); }
void watch_close(void* w) { delete static_cast<Watch*>(w); }
bool watch_frame(void* w, std::span<const uint8_t> bytes, std::string& error) {
  return keep(*static_cast<Watch*>(w), bytes, error);
}
int watch_players(const void*) { return kSequelViewSlots; }
void watch_view(const void* w, int player, std::vector<uint8_t>& out) {
  const Watch& watch = *static_cast<const Watch*>(w);
  if (player < 0 || player >= kSequelViewSlots || watch.views[static_cast<size_t>(player)].empty())
    out.assign(1, 0);
  else
    out = watch.views[static_cast<size_t>(player)];
}

}  // namespace

void sequel_player_view(const sv::RulesCommon& c, int slot, std::vector<uint8_t>& out) {
  const sv::RulesWorld& world = c.world;
  if (slot < 0 || slot >= kSequelViewSlots || slot >= world.max_clients ||
      static_cast<size_t>(slot) + 1 >= world.entities.size() || !world.clients[slot].active ||
      world.clients[slot].spectator) {
    out.push_back(0);
    return;
  }
  const sv::WorldEntity& e = world.entities[static_cast<size_t>(slot) + 1];
  const sv::PlayerNumbers& n = world.clients[slot].numbers;
  out.push_back(1);
  for (int i = 0; i < 3; i++) put16(out, eighths(e.origin[i]));
  for (int i = 0; i < 3; i++) put16(out, turn16(e.view_angles[i]));
  out.push_back(static_cast<uint8_t>(e.modelindex));
  put16(out, e.frame);
  out.push_back(static_cast<uint8_t>(e.skin));
  put16(out, e.effects);
  out.push_back(static_cast<uint8_t>(e.carried_model));
  put16(out, static_cast<int32_t>(n.health));
  put16(out, static_cast<int32_t>(n.frags));
  out.push_back(static_cast<uint8_t>(n.weapon_model));
  out.push_back(static_cast<uint8_t>(static_cast<int32_t>(n.weapon_frame)));
  out.push_back(static_cast<uint8_t>(e.waterlevel));
  for (const int16_t s : n.stats) put16(out, s);
}

void sequel_tv_state(const sv::RulesCommon& c, std::vector<uint8_t>& out) {
  for (int k = 0; k < kSequelViewSlots; k++) sequel_player_view(c, k, out);
}

void sequel_watch_hooks(ge::server::ServerGame& g) {
  g.watch_open = watch_open;
  g.watch_close = watch_close;
  g.watch_keyframe = watch_frame;  // a frame is whole: a keyframe is the same bytes
  g.watch_frame = watch_frame;
  g.watch_players = watch_players;
  g.watch_view = watch_view;
}

}  // namespace shooters_edge::net

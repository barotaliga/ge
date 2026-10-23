// THE PLAYERS AND THEIR DEVICES: which pad is whose on a shared screen, and what happens when a
// cable comes out. With one player every device is his (`devices_of` is empty, and `evaluate`
// reads everything), and each device he presses on is noted as his so its loss is heard. With
// more, a device that is not yet anyone's JOINS the first free player
// when a button on it is pressed (a keyboard brings the mouse with it). A player's device that
// is unplugged leaves him WAITING for it (`lost`: the game pauses and says "reconnect"); a device
// of the same model plugged in again goes back to him (`returned`), before anyone else may take
// it. The events say each of these once, so a HUD and a pause card answer them.
//
// ge:summary Players and their devices: join on press, hot-plug, lost and returned, per player
// ge:keywords input players player-slot local-multiplayer split-screen join assign hot-plug
// ge:keywords unplug reconnect lost returned pause controller-disconnected devices_of
// ge:skill input
// ge:test tests/input_players_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "input/state.hpp"

namespace ge::input {

inline constexpr int kMaxPlayers = 8;
inline constexpr int kDevicesPerPlayer = 4;

struct PlayerSlot {
  bool joined = false;
  bool waiting = false;  // a device of his was unplugged and has not come back
  std::array<DeviceId, kDevicesPerPlayer> devices{};
  std::array<std::string, kDevicesPerPlayer> keys{};  // each device's model
  int n = 0;
  std::string lost_key;  // the model of the device he waits for
};

struct PlayerEvent {
  enum Kind : uint8_t { joined, lost, returned } kind = joined;
  int player = 0;
  DeviceId device = 0;
};

struct Players {
  int most = 1;  // how many players the game takes now
  std::array<PlayerSlot, kMaxPlayers> p{};
  std::array<PlayerEvent, size_t{2} * kMaxDevices> events{};  // this frame's
  int events_n = 0;
};

// After the state's update: joins, losses and returns.
void players_update(Players& pl, const InputState& s);
// The devices player `i` reads; empty (every device) when the game has one player.
std::span<const DeviceId> devices_of(const Players& pl, int i);
// Which player holds the device (-1: no one).
int owner_of(const Players& pl, DeviceId id);
// Whether the player reads the pooled keyboard and mouse.
bool has_keyboard(const Players& pl, const InputState& s, int i);

}  // namespace ge::input

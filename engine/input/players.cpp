// The players' joins, losses and returns.
//
// ge:summary Players: a press joins a device, an unplug leaves him waiting, a replug returns it
// ge:keywords input players join lost returned
// ge:skill input
// ge:test tests/input_players_test.cpp
#include "input/players.hpp"

namespace ge::input {

namespace {

void emit(Players& pl, PlayerEvent::Kind k, int player, DeviceId d) {
  if (pl.events_n < static_cast<int>(pl.events.size())) pl.events[pl.events_n++] = {k, player, d};
}

void give(PlayerSlot& p, DeviceId d, const std::string& key) {
  if (p.n >= kDevicesPerPlayer) return;
  p.devices[p.n] = d;
  p.keys[p.n] = key;
  p.n++;
}

// Take the device off the player; its model's key.
std::string take(PlayerSlot& p, DeviceId d) {
  for (int i = 0; i < p.n; i++) {
    if (p.devices[i] == d) {
      std::string key = p.keys[i];
      p.n--;
      p.devices[i] = p.devices[p.n];
      p.keys[i] = p.keys[p.n];
      p.devices[p.n] = 0;
      p.keys[p.n].clear();
      return key;
    }
  }
  return {};
}

bool pressed_on(const InputState& s, const Slot& slot) {
  switch (slot.info.kind) {
    case DeviceKind::keyboard:
      return s.keys_pressed.any() && s.last_used == slot.info.id;
    case DeviceKind::mouse:
      return s.mouse.buttons.pressed != 0;
    default:
      return slot.buttons.pressed != 0;
  }
}

}  // namespace

int owner_of(const Players& pl, DeviceId id) {
  for (int i = 0; i < kMaxPlayers; i++) {
    for (int k = 0; k < pl.p[i].n; k++) {
      if (pl.p[i].devices[k] == id) return i;
    }
  }
  return -1;
}

void players_update(Players& pl, const InputState& s) {
  pl.events_n = 0;
  for (int r = 0; r < s.removed_n; r++) {
    const int who = owner_of(pl, s.removed[r]);
    if (who < 0) continue;
    pl.p[who].lost_key = take(pl.p[who], s.removed[r]);
    pl.p[who].waiting = true;
    emit(pl, PlayerEvent::lost, who, s.removed[r]);
  }
  for (int a = 0; a < s.added_n; a++) {
    const Slot* slot = find_slot(s, s.added[a]);
    if (!slot) continue;
    for (int i = 0; i < pl.most && i < kMaxPlayers; i++) {
      PlayerSlot& p = pl.p[i];
      if (p.waiting && p.lost_key == slot->info.key) {
        give(p, slot->info.id, slot->info.key);
        p.waiting = false;
        emit(pl, PlayerEvent::returned, i, slot->info.id);
        break;
      }
    }
  }
  for (const Slot& slot : s.slots) {
    if (!slot.active || owner_of(pl, slot.info.id) >= 0 || !pressed_on(s, slot)) continue;
    if (slot.info.kind == DeviceKind::mouse) continue;  // the mouse joins with a keyboard
    // One player takes every device he presses on; more take one each, in turn.
    int who = -1;
    for (int i = 0; i < pl.most && i < kMaxPlayers && who < 0; i++) {
      if (pl.most <= 1 || !pl.p[i].joined) who = i;
    }
    if (who < 0) continue;
    PlayerSlot& p = pl.p[who];
    const bool first = !p.joined;
    p.joined = true;
    give(p, slot.info.id, slot.info.key);
    if (slot.info.kind == DeviceKind::keyboard && s.mouse_id && owner_of(pl, s.mouse_id) < 0) {
      const Slot* mouse = find_slot(s, s.mouse_id);
      give(p, s.mouse_id, mouse ? mouse->info.key : std::string());
    }
    if (first || pl.most > 1) emit(pl, PlayerEvent::joined, who, slot.info.id);
  }
}

std::span<const DeviceId> devices_of(const Players& pl, int i) {
  if (pl.most <= 1 || i < 0 || i >= kMaxPlayers) return {};
  return {pl.p[i].devices.data(), static_cast<size_t>(pl.p[i].n)};
}

bool has_keyboard(const Players& pl, const InputState& s, int i) {
  if (pl.most <= 1) return true;
  return owner_of(pl, s.keyboard) == i;
}

}  // namespace ge::input

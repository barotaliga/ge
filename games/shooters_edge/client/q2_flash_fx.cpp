// Quake II's muzzle flash as the window shows it (q2_flash_fx.hpp).
//
// ge:summary Quake II's muzzle flash into sounds and the look's flash
// ge:keywords q2 muzzle-flash flash-sound
// ge:skill shooter
// ge:test tests/shooters_edge_q2_flash_test.cpp
#include "shooters_edge/client/q2_flash_fx.hpp"

#include <map>
#include <string>

#include "shooters_edge/server/q2_flash.hpp"

namespace shooters_edge::client {

namespace {

namespace sv = shooters_edge::server;

// A sound's name with its variant `k` (0-based) in place of `%d` (1..) or `%c` (a..); the text
// kept for the program's life, as DemoSound::name asks.
const char* variant_name(const char* pattern, int k) {
  static std::map<std::string, std::string> names;
  std::string out(pattern);
  if (const size_t at = out.find("%d"); at != std::string::npos)
    out.replace(at, 2, std::to_string(k + 1));
  else if (const size_t c = out.find("%c"); c != std::string::npos)
    out.replace(c, 2, std::string(1, static_cast<char>('a' + k)));
  return names.try_emplace(out, out).first->second.c_str();
}

}  // namespace

bool q2_flash_fx(int value, bool monster, int entity, sh::Vec3f origin, sh::Vec3f angles, bool own,
                 uint32_t pick, std::vector<DemoSound>& sounds, std::vector<DemoEffect>& effects) {
  const bool silenced = !monster && (value & sv::kQ2FlashSilenced) != 0;
  if (!monster) value &= ~sv::kQ2FlashSilenced;
  const sv::Q2Flash* f = monster ? sv::q2_monster_flash(value) : sv::q2_player_flash(value);
  if (f == nullptr) return false;
  for (const sv::Q2FlashSound& w : f->sounds) {
    if (w.name == nullptr) continue;
    DemoSound d;
    d.entity = entity;
    d.channel = w.channel;
    d.name = w.variants > 0 ? variant_name(w.name, static_cast<int>(pick % uint32_t(w.variants)))
                            : w.name;
    d.volume = silenced ? 0.2f : 1.0f;
    d.attenuation = w.attenuation;
    d.origin = origin;
    d.delay = w.delay;
    sounds.push_back(d);
    pick = pick * 1103515245u + 12345u;  // the next variant drawn on
  }
  if (f->look == nullptr || own) return true;
  const sh::ViewAxes ax = sh::angle_vectors(angles);
  sh::Vec3f at = origin + ax.forward * 18.0f + ax.right * 16.0f;
  if (monster) at = origin + ax.forward * f->ahead + ax.right * f->right + sh::Vec3f{0, 0, f->up};
  DemoEffect e;
  e.kind = DemoEffectKind::named_trail;
  e.name = std::string("te_monster_flash.") + f->look;
  e.entity = entity;
  e.at = at;
  e.end = at + ax.forward * 16.0f;
  effects.push_back(e);
  return true;
}

}  // namespace shooters_edge::client

// Launchers in backpacks (packs.hpp): the call, the fetch and the bore.
//
// ge:summary Launchers in backpacks: an enemy's pack called, fetched by who needs it, the bore
// ge:keywords packs_obituary packs_goal packs_bore packs_hear rl-pack
// ge:skill bots
// ge:test tests/shooters_edge_talk_test.cpp
#include "shooters_edge/bots/packs.hpp"

#include <cmath>
#include <iterator>
#include <string_view>

#include "ai/memory.hpp"
#include "shooter/traits.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/bots/talk.hpp"

namespace shooters_edge::bots {

namespace {

namespace sh = ge::shooter;

constexpr int32_t kWRockets = 32;      // the launcher's W_ bit
constexpr int32_t kRocketsNumber = 7;  // and its impulse
constexpr double kPackSeen_s = 2;      // the victim's place known this lately: where the pack lies
constexpr float kPackReached = 48;     // a called pack this near is had (or gone)
constexpr double kBoreHold_s = 1.5;    // the bore's command held this long at most
constexpr double kPackCall_s = 3;  // a pack's call waits this long at most for its turn to speak

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

sh::Vec3f at_of(const ge::ai::Memory& m) {
  return {static_cast<float>(m.x), static_cast<float>(m.y), static_cast<float>(m.z)};
}

}  // namespace

void packs_hear(Fighter& f, double t) {
  // Every command's sounds (a think's hearing samples fewer): a launch is loud and plain.
  for (const sh::ViewSound& snd : f.bot.view.sounds)
    if (std::string_view(snd.sample) == "weapons/sgun1.wav" && snd.ent >= 1 &&
        static_cast<size_t>(snd.ent) < f.rl_fired.size())
      f.rl_fired[static_cast<size_t>(snd.ent)] = t;
  // A pack call pending (packs_obituary): said once it may speak.
  if (t < f.pack_call_until) {
    Said said;
    said.kind = SayKind::pack;
    said.item = "rl";
    if (mind_say(f, t, said, f.pack_call_at)) {
      f.stats.packs_called++;
      f.pack_call_until = -1e9;
    }
  }
  // Where each one in sight stands: where his pack falls if he dies (the senses drop the dead).
  for (const sh::Contact& c : f.bot.senses.contacts)
    if (c.visible && c.ent >= 1 && static_cast<size_t>(c.ent) < f.last_seen_t.size()) {
      f.last_seen_at[static_cast<size_t>(c.ent)] = at_of(c.seen);
      f.last_seen_t[static_cast<size_t>(c.ent)] = t;
    }
}

void packs_obituary(Fighter& f, const server::RulesCommon& s, const std::string& line, double t) {
  if (!f.team || f.name.empty() || line.find(f.name, 1) == std::string::npos) return;
  // The victim: the player whose name the line starts with, not this one, not a mate.
  for (size_t i = 0; i < std::size(s.world.clients); i++) {
    const std::string& name = s.world.clients[i].name;
    if (name.empty() || name == f.name || line.size() <= name.size() + 1 ||
        line.compare(0, name.size(), name) != 0 || line[name.size()] != ' ')
      continue;
    for (const Mate& m : f.mind.mates)
      if (m.name == name) return;  // a mate's death
    const int ent = static_cast<int>(i) + 1;
    if (static_cast<size_t>(ent) >= f.rl_fired.size() ||
        t - f.rl_fired[static_cast<size_t>(ent)] > kRlHeard_s)
      return;  // no rocket of his heard lately: no launcher to call
    if (t - f.last_seen_t[static_cast<size_t>(ent)] > kPackSeen_s) return;  // not seen fall
    // Said at its next chance to speak (a line a few seconds apart at most): pending till then.
    f.pack_call_at = f.last_seen_at[static_cast<size_t>(ent)];
    f.pack_call_until = t + kPackCall_s;
    return;
  }
}

int32_t packs_goal(Fighter& f, sh::Vec3f me, const Stats& st, double t) {
  Mind& mn = f.mind;
  if (t - mn.pack_t > kPackFresh_s || !f.bot.pilot.pilot.g) return -1;
  if (dist(me, mn.pack_at) < kPackReached) {
    mn.pack_t = -1e9;  // had, or gone
    return -1;
  }
  const bool lack = (st.items & kWRockets) == 0;
  if (mn.pack_rl ? !lack : false) return -1;  // a launcher of its own: the pack is a mate's
  // The quad's carrier goes; else only the one who believes himself the nearest of his team.
  const bool quad = (st.items & 4194304) != 0;
  if (!quad) {
    const float mine = dist(me, mn.pack_at);
    for (const Mate& m : mn.mates)
      if (t - m.at_t < 5 && dist(m.at, mn.pack_at) < mine && !(m.has_rl && t - m.weapon_t < 20))
        return -1;
  }
  f.stats.packs_fetched++;
  return sh::nav_nearest(*f.bot.pilot.pilot.g, mn.pack_at);
}

bool packs_bore(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
                sh::MoveCmd& cmd) {
  const sh::Vec3f at = s.world.entities[static_cast<size_t>(me)].origin;
  if (t >= f.bore_until) {
    if (!f.team || f.teamwork == MapSense::novice || sh::selfish(f.traits)) return false;
    if ((st.items & kWRockets) == 0 || st.rockets < 1 || t - f.bored_at < kBoreGap_s) return false;
    if (st.health + st.armor * st.armortype > kBoreWeak) return false;  // its rocket would not do
    bool carrier = false;
    for (const Mate& m : f.mind.mates)
      carrier = carrier || (m.quad && t - m.quad_t < 25 && t - m.at_t < 5 &&
                            dist(m.at, at) < kBoreNear && !(m.has_rl && t - m.weapon_t < 20));
    if (!carrier) return false;
    Said said;
    said.kind = SayKind::pack;
    said.item = "rl";
    mind_say(f, t, said, at);
    f.bore_until = t + kBoreHold_s;
    f.bored_at = t;
    f.stats.bored++;
  }
  // The launcher in hand, the view at its own feet, still; fired once it is up.
  if (st.weapon != kWRockets) {
    cmd.impulse = kRocketsNumber;
  } else {
    cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
  }
  cmd.angles.x = sh::quantize_angle(89);
  cmd.forward = cmd.side = 0;
  return true;
}

}  // namespace shooters_edge::bots

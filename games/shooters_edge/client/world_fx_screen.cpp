// THE SCREEN'S TINTS (client/world_fx.hpp § step_screen): the liquid round the eye, a hit's flash,
// a pickup's flash and the power held. A single-player program's player takes the original's tints
// (effects::Screen::classic: the blow's colour by armour and blood, the bonus flash its stuffed
// `bf` starts, one power at a time, the contents of the map at the eye); a match keeps ours (the
// health lost, the power and shield glows, the water level).
//
// ge:summary The screen's tints each frame: liquids, hits, the pickup flash, powers; the classic's
// ge:keywords screen-blend tint damage-flash bonus-flash powerup-tint contents-tint step_screen
// ge:keywords classic-tints quad suit ring pentagram
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#include <algorithm>

#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/client/world_models.hpp"

namespace shooters_edge::client {

namespace fx = shooters_edge::effects;

namespace {

// The mod's contents codes for the liquids (games/shooters_edge/rules/common_decl.hpp's CONTENT_*).
constexpr float kWater = -3, kSlime = -4, kLava = -5;
constexpr float kEyesUnder = 3;  // waterlevel: the eyes under the surface
constexpr int kContentsWater = -3, kContentsSlime = -4, kContentsLava = -5;
// A single-player program's power bits (its `items`).
constexpr int kItemRing = 524288, kItemShield = 1048576, kItemSuit = 2097152, kItemQuad = 4194304;

}  // namespace

void step_screen(const FxScene& l, FxState& s, double dt, int mine, FxFrame& out) {
  const PlayerState& p = l.me;
  // The screen: the liquid round the eye, and a hit's flash from the blows reported (a campaign)
  // or the health lost (a match).
  s.hurt = fx::fade_hurt(s.hurt, dt);
  s.armour = fx::fade_hurt(s.armour, dt);
  if (l.blows.size() < s.blows_seen) {  // a new map: every tint starts clear
    s.blows_seen = 0;
    if (l.campaign) s.hurt = s.armour = s.bonus = 0;
  }
  // An item taken: the gold flash, lit again by each one (a count gone back: a new map or demo).
  if (l.bonuses < s.bonuses_seen) s.bonuses_seen = l.bonuses;
  s.bonus = fx::fade_bonus(s.bonus, dt, l.bonuses - s.bonuses_seen);
  s.bonuses_seen = l.bonuses;
  if (l.campaign) {
    // The single-player program's player: the original's tints (effects::Screen::classic).
    for (; s.blows_seen < l.blows.size(); s.blows_seen++)
      fx::take_classic_hurt(s.hurt, s.hurt_rgb, l.blows[s.blows_seen].armor,
                            l.blows[s.blows_seen].blood);
    fx::Screen screen;
    screen.classic = true;
    screen.hurt = s.hurt;
    for (int c = 0; c < 3; c++) screen.hurt_rgb[c] = s.hurt_rgb[c];
    screen.bonus = s.bonus;
    const int items = l.own > 0 ? l.me_items : 0;
    screen.powerup = (items & kItemQuad) != 0     ? fx::Powerup::quad
                     : (items & kItemSuit) != 0   ? fx::Powerup::suit
                     : (items & kItemRing) != 0   ? fx::Powerup::ring
                     : (items & kItemShield) != 0 ? fx::Powerup::pent
                                                  : fx::Powerup::none;
    // By the map's contents where the eye is (dead, in noclip and at the intermission too).
    const int at = l.own > 0 ? l.eye_contents : 0;
    screen.medium = at == kContentsLava    ? fx::Medium::lava
                    : at == kContentsSlime ? fx::Medium::slime
                    : at == kContentsWater ? fx::Medium::water
                                           : fx::Medium::air;
    s.health = p.health;
    fx::screen_blend(screen, out.blend);
    out.screen = screen;
    if (screen.medium == fx::Medium::lava || screen.medium == fx::Medium::slime)
      screen.medium = fx::Medium::air;
    fx::screen_blend(screen, out.blend_dry);
    out.screen_dry = screen;
    return;
  }
  if (l.own > 0 && !p.dead && p.health < s.health) {
    s.hurt = fx::take_hurt(s.hurt, s.health - p.health);
  }
  s.health = p.health;
  fx::Screen screen;
  screen.hurt = s.hurt;
  screen.bonus = s.bonus;
  screen.armour = s.armour;
  screen.power = (mine & kGlowBlue) != 0;
  screen.shield = (mine & kGlowRed) != 0;
  if (l.own > 0 && l.waterlevel >= kEyesUnder) {
    const float type = l.watertype;
    screen.medium = type == kLava    ? fx::Medium::lava
                    : type == kSlime ? fx::Medium::slime
                    : type == kWater ? fx::Medium::water
                                     : fx::Medium::air;
  }
  fx::screen_blend(screen, out.blend);
  out.screen = screen;
  if (screen.medium == fx::Medium::lava || screen.medium == fx::Medium::slime)
    screen.medium = fx::Medium::air;
  fx::screen_blend(screen, out.blend_dry);
  out.screen_dry = screen;
}

}  // namespace shooters_edge::client

// THE FIGHTER SET UP (fighter.hpp `read_stats`, `fighter_pilot`, `fighter_init`): a player's
// stats read off what is sent, the pilot's settings from the features and the moves trait, and a
// fighter made ready on a map: its senses by the traits, its keyboard, the map's items found among
// the entities sent, the items' rules, the buttons, exits, ears and spam, and the features'
// switches.
//
// ge:summary The fighter set up: stats read, the pilot's settings, the map's items, the switches
// ge:keywords fighter read_stats fighter_pilot fighter_init pilot-settings features traits
// ge:keywords item-spot entity-text setup init
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "progs/entity_text.hpp"
#include "shooters_edge/bots/buttons.hpp"
#include "shooters_edge/bots/ears.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/hop_start_book.hpp"
#include "shooters_edge/bots/item_routes.hpp"
#include "shooters_edge/bots/keyboard.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/spam.hpp"

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;

constexpr float bt_gap_past = 160;  // `gappast`: a gap's jump lands this far on past its node
// What a fighter sees: a player's screen, 120 degrees across and as high as a 16:9 monitor shows
// at that width (2 atan(tan 60 * 9/16)), people's own field of view.
constexpr float kViewWide = 120;
constexpr float kViewHigh = 88.5f;

}  // namespace

Stats read_stats(const ge::shooter::BotView& s, int ent, const BotArsenal& arsenal) {
  const ge::shooter::PlayerNumbers& n = s.world.clients[static_cast<size_t>(ent) - 1].numbers;
  Stats st;
  st.arsenal = &arsenal;
  st.health = n.health;
  st.armor = n.armour;
  st.armortype = n.armour_type;
  arsenal.read(n, st);
  return st;
}

sh::PilotSettings fighter_pilot(const sh::Features& features, const sh::Traits& traits,
                                sh::AimSkill skill, sh::PilotHands& hands) {
  // Moves: an expert's the bunny hop on practised hands (shooter/pilot.hpp); an average player's
  // the simple hop (forward and the side key of the turn) on steady hands; a novice's a walk and a
  // strafe on a person's hands.
  const sh::AimSkill moves = sh::tier_of(traits[sh::Trait::moves]);
  const bool expert = moves == sh::AimSkill::expert;
  const bool average = moves == sh::AimSkill::average;
  sh::PilotSettings ps;
  ps.hop = (expert || average) && features[sh::Feature::hop];
  ps.hop_simple = average && !features[sh::Feature::avghop];
  // (`hopsee`) The hop's arc checked for a wall further ahead: at 600+ units/s the 0.12 s look
  // saw a corner's outer wall too late to turn from it.
  if (features[sh::Feature::hopsee]) ps.hop_see_s = 0.25f;
  // (`hoparc`) The side held in the air until the aim lies further the other way: one wide arc a
  // jump, the way the maintainer hops.
  if (features[sh::Feature::hoparc]) ps.hop_air_hold = 25;
  // (`hopland`) The side swapped only at the landing, the flight's turn paced to it.
  ps.hop_land = features[sh::Feature::hopland];
  // (`hoprhythm`) People's rhythm in the air: a swap kept 0.1 s, straight on when both arcs wall,
  // the side set with the jump.
  if (features[sh::Feature::hoprhythm]) {
    ps.hop_air_keep_s = 0.1f;
    ps.hop_straight = true;
    ps.hop_wall_ground = true;
  }
  ps.hop_stairs_walked = features[sh::Feature::stairs];
  ps.slopes = features[sh::Feature::slopes];
  ps.backstep = features[sh::Feature::backstep];
  ps.runup_keys = features[sh::Feature::runupkeys];
  ps.jump_fly = features[sh::Feature::jumpfly];
  ps.gap_past = features[sh::Feature::gappast] ? bt_gap_past : 0;
  ps.lip_jump = features[sh::Feature::lipjump];
  ps.gap_strafe = features[sh::Feature::airgap];
  ps.pilot_cast = features[sh::Feature::pilotcast];
  ps.gap_circle = features[sh::Feature::gapbuild];
  ps.gap_lava = features[sh::Feature::gaplava];
  ps.shut_every = features[sh::Feature::bridgeseen];
  ps.hop_side_off = features[sh::Feature::sidehop];
  ps.brink_line = features[sh::Feature::brinkline];
  ps.brink_side = features[sh::Feature::sidebrink];
  ps.walk_falls = features[sh::Feature::fallseen];
  ps.lip_look_s = features[sh::Feature::liplook] ? 0.3f : 0;
  ps.jumper.lip_share = features[sh::Feature::sidebrink] ? kLipShare : 0;
  ps.curve_keys = features[sh::Feature::keys] && features[sh::Feature::curvekeys];
  ps.under_way = features[sh::Feature::underway];
  ps.lift_off = features[sh::Feature::liftoff];
  ps.rocket_wait = features[sh::Feature::rjready];
  ps.retarget = features[sh::Feature::retarget];
  ps.hop_headroom = features[sh::Feature::headroom];
  ps.hop_lips = features[sh::Feature::liphop];
  // The circle jump by skill: an expert curves up to 0.3 s for ~430 (people's standing jumps
  // take off at 433, p50), an average player a shorter curve for less (people's p25: 377).
  ps.hop_circle = features[sh::Feature::circlejump];
  ps.hop_chain = features[sh::Feature::chainhop];
  ps.hop_land_wall_s = features[sh::Feature::landwall] ? 0.15f : 0.f;
  ps.hop_jump_held = features[sh::Feature::hopjump];
  ps.hop_side_only = features[sh::Feature::hopside];
  ps.hop_open = features[sh::Feature::hopopen];
  ps.aim_land_s = features[sh::Feature::aimland] ? 0.3f : 0.f;
  ps.recipe_heading = features[sh::Feature::starthead];
  ps.recipe_clear = features[sh::Feature::startclear];
  ps.gap_hold = features[sh::Feature::gaphold];
  ps.lift_brake = features[sh::Feature::liftbrake];
  ps.walk_mash = features[sh::Feature::walkmash];
  ps.give_up = features[sh::Feature::giveup];
  ps.stair_free = features[sh::Feature::stairbump];
  ps.mash_start = features[sh::Feature::mashstart];
  ps.straight_fly = features[sh::Feature::straightfly];
  ps.mash_jump = features[sh::Feature::mashjump];
  ps.aim_ground = features[sh::Feature::aimground];
  ps.script_fresh = features[sh::Feature::scriptfresh];
  ps.gap_turn = features[sh::Feature::gapturn];
  ps.gap_near = features[sh::Feature::gapnear];
  ps.script_entry = features[sh::Feature::scriptentry];
  ps.circle.target = expert ? 430.f : 380.f;
  ps.circle.most_s = expert ? 0.3f : 0.2f;
  // People's hop starts (`recipes`): a hopper copies its level's out of the book.
  if (ps.hop && features[sh::Feature::recipes]) ps.recipes = hop_start_book();
  ps.recipe_level = expert ? 2 : average ? 1 : 0;
  ps.recipe_keys_s = features[sh::Feature::keys] ? (expert ? 0.03f : average ? 0.04f : 0.05f) : 0.f;
  ps.brink = features[sh::Feature::brink];
  ps.ahead = features[sh::Feature::ahead];
  ps.ahead_bodies = features[sh::Feature::bodies];
  // Its lane off the route's line (shooter/pilot.hpp): a person never walks the line; the less
  // practised wander wider. A perfect player keeps to it.
  ps.lane = skill == sh::AimSkill::perfect || !features[sh::Feature::lane] ? 0.f
            : expert                                                       ? 0.45f
            : average                                                      ? 0.6f
                                                                           : 0.75f;
  ps.air_steer = features[sh::Feature::airsteer];
  hands = expert    ? sh::PilotHands::practised
          : average ? sh::PilotHands::steady
                    : sh::PilotHands::human;
  return ps;
}

void fighter_init(Fighter& f, const sh::NavGraph& g, const ge::shooter::BotView& s, int slot,
                  const char* entities, sh::AimSkill skill, const sh::Traits& traits, double hz,
                  uint32_t seed) {
  sh::PilotHands hands = sh::PilotHands::none;
  const sh::PilotSettings ps = fighter_pilot(f.features, traits, skill, hands);
  // The aim: the scattergun's to start with, another weapon's taken up as it is
  // (`fighter_command`).
  f.aim_slot = 0;
  sh::bot_init(f.bot, g, ps, hands,
               skill == sh::AimSkill::perfect
                   ? sh::aim_profile(skill)
                   : sh::aim_profile(traits[sh::Trait::aim_sg], traits[sh::Trait::reaction]),
               hz, seed);
  f.slot = slot;
  f.skill = skill;
  f.traits = traits;
  f.temper = sh::temper_of(traits);
  f.sense = map_sense_of(sh::tier_of(traits[sh::Trait::knowledge]));
  f.timing = map_sense_of(sh::tier_of(traits[sh::Trait::timing]));
  f.comms = map_sense_of(sh::tier_of(traits[sh::Trait::comms]));
  f.teamwork = map_sense_of(sh::tier_of(traits[sh::Trait::teamplay]));
  f.hearing = map_sense_of(sh::tier_of(traits[sh::Trait::hearing]));
  f.camp = map_sense_of(sh::tier_of(traits[sh::Trait::camping]));
  f.rash = sh::tier_of(traits[sh::Trait::discipline]) == sh::AimSkill::novice;
  f.edge = sh::edge_of(sh::mean_aim(traits));
  f.hear_react_s =
      f.bot.aim.react_med > 0
          ? f.bot.aim.react_med +
                static_cast<float>(sh::scale(traits[sh::Trait::reaction], {0.2, 0.15, 0.1, 0.08}))
          : 0;
  f.rng = ge::core::make_rng(seed * 2654435761u + 12345u);
  f.keys_rng = ge::core::make_rng(seed * 2246822519u + 777u);
  f.clean_keys = static_cast<float>(sh::scale(traits[sh::Trait::moves], {0.08, 0.2, 0.4, 0.5}));
  fighter_keyboard_init(f, traits[sh::Trait::moves], seed);
  sh::dodge_init(f.dodge, seed);
  f.press_rng = ge::core::make_rng(seed ^ 0x2545f491u);
  f.dodge_k = sh::dodge_skill(traits[sh::Trait::dodge]);
  const int me = slot + 1;
  sh::client_view(s.sv, me, f.bot.view);
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  f.items.clear();
  if (!pg::parse_entity_text(entities, blocks, error)) return;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    if (!cn) continue;
    const char* flags = pg::block_value(b, "spawnflags");
    ItemSpot it;
    if (!item_of(cn, flags ? std::atoi(flags) : 0, it, *f.arsenal)) continue;
    sh::Vec3f origin;
    parse_vec(pg::block_value(b, "origin"), origin);
    // The entity sent nearest the map's place, across the floor.
    float best = 48;
    for (const sh::ViewEntity& v : f.bot.view.entities) {
      if (v.ent <= s.sv.set.maxclients) continue;
      const ge::shooter::WorldEntity& e = s.world.entities[static_cast<size_t>(v.ent)];
      // The middle of its box on the floor; its origin when its row carries no box (Quake II's).
      const bool boxed = e.absmax.x > e.absmin.x || e.absmax.z > e.absmin.z;
      const sh::Vec3f mid = boxed ? sh::Vec3f{(e.absmin.x + e.absmax.x) * 0.5f,
                                              (e.absmin.y + e.absmax.y) * 0.5f, e.absmin.z}
                                  : e.origin;
      const float dx = v.origin.x - origin.x, dy = v.origin.y - origin.y;
      const float d = std::sqrt(dx * dx + dy * dy);
      if (d < best) {
        best = d;
        it.ent = v.ent;
        it.at = mid;
      }
    }
    if (it.ent < 0) continue;
    it.node = sh::nav_nearest(g, {it.at.x, it.at.y, it.at.z + 24});
    f.items.push_back(it);
  }
  // The map's item rules (item_routes.hpp): an item no goal while another is to be had first.
  for (const ItemRule& r : item_rules(s.map_name))
    for (ItemSpot& it : f.items) {
      const float dx = it.at.x - r.x, dy = it.at.y - r.y, dz = it.at.z - r.z;
      if (std::strcmp(item_call(it), r.item) != 0 || dx * dx + dy * dy + dz * dz > 48 * 48)
        continue;
      for (size_t q = 0; q < f.items.size(); q++)
        if (std::strcmp(item_call(f.items[q]), r.after) == 0) {
          it.after = static_cast<int32_t>(q);
          it.after_due_s = r.due_s;
          break;
        }
    }
  knowledge_init(f);
  buttons_init(f, g, s, entities);
  exits_init(f, g, s, entities);
  ears_init(f, sh::tier_of(traits[sh::Trait::hearing]), entities);
  spam_init(f, entities);
  // A screen 120 degrees across (`screen`), or the round cone of the senses' own setting.
  if (f.features[sh::Feature::screen]) {
    f.bot.senses.set.fov = kViewWide;
    f.bot.senses.set.fov_y = kViewHigh;
  }
  f.bot.keep = f.features[sh::Feature::keep];
  f.bot.aimhop = f.features[sh::Feature::aimhop];
  f.bot.airkeys = f.features[sh::Feature::airkeys];
  f.bot.airview = f.features[sh::Feature::airview];
  f.bot.airturn = f.features[sh::Feature::airturn];
  f.bot.keyswap = f.features[sh::Feature::keyswap];
  f.bot.hopturn = f.features[sh::Feature::hopturn];
  f.bot.hoppitch = f.features[sh::Feature::hoppitch];
  f.bot.airwall = f.features[sh::Feature::airwall];
  f.bot.airwalk = f.features[sh::Feature::airwalk];
  f.bot.airarc = f.features[sh::Feature::airarc];
  f.bot.hopside = f.features[sh::Feature::hopside];
  f.bot.aimstrafe = f.features[sh::Feature::aimstrafe];
  f.bot.mashview = f.features[sh::Feature::mashview];
  f.bot.levelview = f.features[sh::Feature::levelview];
  f.bot.paceback = f.features[sh::Feature::paceback];
  f.bot.circlekeys = f.features[sh::Feature::circlekeys];
  f.bot.circleview = f.features[sh::Feature::circleview];
  f.bot.circleaim = f.features[sh::Feature::circleaim];
  f.bot.leadwall = f.features[sh::Feature::leadwall];
  f.bot.leadmean = f.features[sh::Feature::leadmean];
  f.bot.leadarc = f.features[sh::Feature::leadarc];
  f.bot.paced = f.features[sh::Feature::pace];
  f.bot.trial = f.features[sh::Feature::trial];
  // A change of course read a reaction late (`eye`), and the aim from its eye ahead (`selfaim`).
  f.bot.senses.set.course_s = f.features[sh::Feature::eye] ? f.bot.aim.react_med : 0;
  f.bot.selfaim = f.features[sh::Feature::selfaim];
  f.bot.corner = f.features[sh::Feature::corner];
  f.bot.glance = f.features[sh::Feature::glance];
  f.bot.lookway = f.features[sh::Feature::lookway];
  f.bot.calm_vel = f.features[sh::Feature::calmvel];
  f.bot.look_turn = f.features[sh::Feature::lookturn];
  f.bot.view_booked = f.features[sh::Feature::viewbook];
  f.bot.listen = f.features[sh::Feature::listen];
  f.bot.pools = f.bot.pilot.pilot.set.floor_swim = f.features[sh::Feature::pools];
}

}  // namespace shooters_edge::bots

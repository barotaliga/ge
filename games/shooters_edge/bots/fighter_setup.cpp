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

namespace sb = ge::shooterbots;

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;

constexpr float bt_gap_past = 160;  // `gappast`: a gap's jump lands this far on past its node
// What a fighter sees: a player's screen, 120 degrees across and as high as a 16:9 monitor shows
// at that width (2 atan(tan 60 * 9/16)), people's own field of view.
constexpr float kViewWide = 120;
constexpr float kViewHigh = 88.5f;

}  // namespace

Stats read_stats(const ge::shooterbots::BotView& s, int ent, const BotArsenal& arsenal) {
  const ge::shooter::PlayerNumbers& n = s.world.clients[static_cast<size_t>(ent) - 1].numbers;
  Stats st;
  st.arsenal = &arsenal;
  st.health = n.health;
  st.armor = n.armour;
  st.armortype = n.armour_type;
  arsenal.read(n, st);
  return st;
}

sb::PilotSettings fighter_pilot(const sb::Features& features, const sb::Traits& traits,
                                sb::AimSkill skill, sb::PilotHands& hands) {
  // Moves: an expert's the bunny hop on practised hands (shooterbots/pilot.hpp); an average
  // player's the simple hop (forward and the side key of the turn) on steady hands; a novice's a
  // walk and a strafe on a person's hands.
  const sb::AimSkill moves = sb::tier_of(traits[sb::Trait::moves]);
  const bool expert = moves == sb::AimSkill::expert;
  const bool average = moves == sb::AimSkill::average;
  sb::PilotSettings ps;
  ps.hop = (expert || average) && features[sb::Feature::hop];
  ps.hop_simple = average && !features[sb::Feature::avghop];
  // (`hopsee`) The hop's arc checked for a wall further ahead: at 600+ units/s the 0.12 s look
  // saw a corner's outer wall too late to turn from it.
  if (features[sb::Feature::hopsee]) ps.hop_see_s = 0.25f;
  // (`hoparc`) The side held in the air until the aim lies further the other way: one wide arc a
  // jump, the way the maintainer hops.
  if (features[sb::Feature::hoparc]) ps.hop_air_hold = 25;
  // (`hopland`) The side swapped only at the landing, the flight's turn paced to it.
  ps.hop_land = features[sb::Feature::hopland];
  // (`hoprhythm`) People's rhythm in the air: a swap kept 0.1 s, straight on when both arcs wall,
  // the side set with the jump.
  if (features[sb::Feature::hoprhythm]) {
    ps.hop_air_keep_s = 0.1f;
    ps.hop_straight = true;
    ps.hop_wall_ground = true;
  }
  ps.hop_swap_ground = features[sb::Feature::swapground];           // the aim's swap at the landing
  if (features[sb::Feature::wallplan]) ps.hop_see_ground_s = 0.5f;  // a flight ahead, jumping
  ps.hop_stairs_walked = features[sb::Feature::stairs];
  ps.slopes = features[sb::Feature::slopes];
  ps.backstep = features[sb::Feature::backstep];
  ps.runup_keys = features[sb::Feature::runupkeys];
  ps.jump_fly = features[sb::Feature::jumpfly];
  ps.gap_past = features[sb::Feature::gappast] ? bt_gap_past : 0;
  ps.lip_jump = features[sb::Feature::lipjump];
  ps.gap_strafe = features[sb::Feature::airgap];
  ps.pilot_cast = features[sb::Feature::pilotcast];
  ps.gap_circle = features[sb::Feature::gapbuild];
  ps.gap_lava = features[sb::Feature::gaplava];
  ps.shut_every = features[sb::Feature::bridgeseen];
  ps.hop_side_off = features[sb::Feature::sidehop];
  ps.brink_line = features[sb::Feature::brinkline];
  ps.brink_side = features[sb::Feature::sidebrink];
  ps.walk_falls = features[sb::Feature::fallseen];
  ps.lip_look_s = features[sb::Feature::liplook] ? 0.3f : 0;
  ps.jumper.lip_share = features[sb::Feature::sidebrink] ? kLipShare : 0;
  ps.jumper.wall_share = features[sb::Feature::midroute] ? kWallShare : 0;
  ps.curve_keys = features[sb::Feature::keys] && features[sb::Feature::curvekeys];
  ps.under_way = features[sb::Feature::underway];
  ps.lift_off = features[sb::Feature::liftoff];
  ps.rocket_wait = features[sb::Feature::rjready];
  ps.retarget = features[sb::Feature::retarget];
  ps.hop_headroom = features[sb::Feature::headroom];
  ps.hop_lips = features[sb::Feature::liphop];
  // The circle jump by skill: an expert curves up to 0.3 s for ~430 (people's standing jumps
  // take off at 433, p50), an average player a shorter curve for less (people's p25: 377).
  ps.hop_circle = features[sb::Feature::circlejump];
  ps.hop_chain = features[sb::Feature::chainhop];
  ps.hop_land_wall_s = features[sb::Feature::landwall] ? 0.35f : 0.f;
  ps.hop_air_end = features[sb::Feature::airend];
  ps.hop_air_plan = features[sb::Feature::airplan];
  ps.hop_jump_held = features[sb::Feature::hopjump];
  ps.hop_side_only = features[sb::Feature::hopside];
  ps.hop_open = features[sb::Feature::hopopen];
  ps.aim_land_s = features[sb::Feature::aimland] ? 0.3f : 0.f;
  ps.recipe_heading = features[sb::Feature::starthead];
  ps.recipe_clear = features[sb::Feature::startclear];
  if (features[sb::Feature::startease]) ps.recipe_ease_s = 0.25f;  // eased in from its own view
  ps.gap_hold = features[sb::Feature::gaphold];
  ps.lift_brake = features[sb::Feature::liftbrake];
  ps.walk_mash = features[sb::Feature::walkmash];
  ps.give_up = features[sb::Feature::giveup];
  ps.stair_free = features[sb::Feature::stairbump];
  ps.mash_start = features[sb::Feature::mashstart];
  ps.straight_fly = features[sb::Feature::straightfly];
  ps.aim_ground = features[sb::Feature::aimground];
  ps.script_fresh = features[sb::Feature::scriptfresh];
  ps.gap_turn = features[sb::Feature::gapturn];
  ps.gap_near = features[sb::Feature::gapnear];
  ps.script_entry = features[sb::Feature::scriptentry];
  ps.circle.target = expert ? 430.f : 380.f;
  ps.circle.most_s = expert ? 0.3f : 0.2f;
  // People's hop starts (`recipes`): a hopper copies its level's out of the book.
  if (ps.hop && features[sb::Feature::recipes]) ps.recipes = hop_start_book();
  ps.recipe_level = expert ? 2 : average ? 1 : 0;
  ps.recipe_keys_s = features[sb::Feature::keys] ? (expert ? 0.03f : average ? 0.04f : 0.05f) : 0.f;
  ps.brink = features[sb::Feature::brink];
  ps.ahead = features[sb::Feature::ahead];
  ps.ahead_bodies = features[sb::Feature::bodies];
  // Its lane off the route's line (shooterbots/pilot.hpp): a person never walks the line; the less
  // practised wander wider. A perfect player keeps to it.
  ps.lane = skill == sb::AimSkill::perfect || !features[sb::Feature::lane] ? 0.f
            : expert                                                       ? 0.45f
            : average                                                      ? 0.6f
                                                                           : 0.75f;
  ps.air_steer = features[sb::Feature::airsteer];
  hands = expert    ? sb::PilotHands::practised
          : average ? sb::PilotHands::steady
                    : sb::PilotHands::human;
  return ps;
}

void fighter_init(Fighter& f, const sb::NavGraph& g, const ge::shooterbots::BotView& s, int slot,
                  const char* entities, sb::AimSkill skill, const sb::Traits& traits, double hz,
                  uint32_t seed) {
  sb::PilotHands hands = sb::PilotHands::none;
  const sb::PilotSettings ps = fighter_pilot(f.features, traits, skill, hands);
  // The aim: the scattergun's to start with, another weapon's taken up as it is
  // (`fighter_command`).
  f.aim_slot = 0;
  sb::bot_init(f.bot, g, ps, hands,
               skill == sb::AimSkill::perfect
                   ? sb::aim_profile(skill)
                   : sb::aim_profile(traits[sb::Trait::aim_sg], traits[sb::Trait::reaction]),
               hz, seed);
  f.slot = slot;
  f.skill = skill;
  f.traits = traits;
  f.temper = sb::temper_of(traits);
  f.sense = map_sense_of(sb::tier_of(traits[sb::Trait::knowledge]));
  f.timing = map_sense_of(sb::tier_of(traits[sb::Trait::timing]));
  f.comms = map_sense_of(sb::tier_of(traits[sb::Trait::comms]));
  f.teamwork = map_sense_of(sb::tier_of(traits[sb::Trait::teamplay]));
  f.hearing = map_sense_of(sb::tier_of(traits[sb::Trait::hearing]));
  f.camp = map_sense_of(sb::tier_of(traits[sb::Trait::camping]));
  f.rash = sb::tier_of(traits[sb::Trait::discipline]) == sb::AimSkill::novice;
  f.edge = sb::edge_of(sb::mean_aim(traits));
  f.hear_react_s =
      f.bot.aim.react_med > 0
          ? f.bot.aim.react_med +
                static_cast<float>(sb::scale(traits[sb::Trait::reaction], {0.2, 0.15, 0.1, 0.08}))
          : 0;
  f.rng = ge::core::make_rng(seed * 2654435761u + 12345u);
  f.keys_rng = ge::core::make_rng(seed * 2246822519u + 777u);
  f.clean_keys = static_cast<float>(sb::scale(traits[sb::Trait::moves], {0.08, 0.2, 0.4, 0.5}));
  fighter_keyboard_init(f, traits[sb::Trait::moves], seed);
  sb::dodge_init(f.dodge, seed);
  f.press_rng = ge::core::make_rng(seed ^ 0x2545f491u);
  f.glance_rng = ge::core::make_rng(seed ^ 0x3c6ef372u);
  f.dodge_k = sb::dodge_skill(traits[sb::Trait::dodge]);
  const int me = slot + 1;
  sb::client_view(s.sv, me, f.bot.view);
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
    // The entity sent nearest the map's place, across the floor (`itemz`: within 48 across, the
    // nearest in height too: dm2 stacks the lob's ledge over a yellow armour, and across alone
    // gave both spots one entity).
    float best = 48, best_score = 1e9f;
    const bool by_height = f.features[sb::Feature::itemz];
    for (const sb::ViewEntity& v : f.bot.view.entities) {
      if (v.ent <= s.sv.set.maxclients) continue;
      const ge::shooter::WorldEntity& e = s.world.entities[static_cast<size_t>(v.ent)];
      // The middle of its box on the floor; its origin when its row carries no box (Quake II's).
      const bool boxed = e.absmax.x > e.absmin.x || e.absmax.z > e.absmin.z;
      const sh::Vec3f mid = boxed ? sh::Vec3f{(e.absmin.x + e.absmax.x) * 0.5f,
                                              (e.absmin.y + e.absmax.y) * 0.5f, e.absmin.z}
                                  : e.origin;
      const float dx = v.origin.x - origin.x, dy = v.origin.y - origin.y;
      const float d = std::sqrt(dx * dx + dy * dy);
      const float score = d + std::fabs(v.origin.z - origin.z);
      if (by_height ? d < 48 && score < best_score : d < best) {
        best = d;
        best_score = score;
        it.ent = v.ent;
        it.at = mid;
      }
    }
    if (it.ent < 0) continue;
    it.node = sb::nav_nearest(g, {it.at.x, it.at.y, it.at.z + 24});
    it.label = it.name + "@" + std::to_string(static_cast<int>(origin.x)) + "," +
               std::to_string(static_cast<int>(origin.y));
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
  ears_init(f, sb::tier_of(traits[sb::Trait::hearing]), entities);
  spam_init(f, entities);
  // A screen 120 degrees across (`screen`), or the round cone of the senses' own setting.
  if (f.features[sb::Feature::screen]) {
    f.bot.senses.set.fov = kViewWide;
    f.bot.senses.set.fov_y = kViewHigh;
  }
  f.bot.keep = f.features[sb::Feature::keep];
  f.bot.aimhop = f.features[sb::Feature::aimhop];
  f.bot.airkeys = f.features[sb::Feature::airkeys];
  f.bot.airview = f.features[sb::Feature::airview];
  f.bot.airturn = f.features[sb::Feature::airturn];
  f.bot.keyswap = f.features[sb::Feature::keyswap];
  f.bot.hopturn = f.features[sb::Feature::hopturn];
  f.bot.hoppitch = f.features[sb::Feature::hoppitch];
  f.bot.airwall = f.features[sb::Feature::airwall];
  f.bot.airwalk = f.features[sb::Feature::airwalk];
  f.bot.airarc = f.features[sb::Feature::airarc];
  f.bot.hopside = f.features[sb::Feature::hopside];
  f.bot.aimstrafe = f.features[sb::Feature::aimstrafe];
  f.bot.levelview = f.features[sb::Feature::levelview];
  f.bot.paceback = f.features[sb::Feature::paceback];
  f.bot.circlekeys = f.features[sb::Feature::circlekeys];
  f.bot.circleview = f.features[sb::Feature::circleview];
  f.bot.circleaim = f.features[sb::Feature::circleaim];
  f.bot.leadwall = f.features[sb::Feature::leadwall];
  f.bot.leadmean = f.features[sb::Feature::leadmean];
  f.bot.leadarc = f.features[sb::Feature::leadarc];
  f.bot.paced = f.features[sb::Feature::pace];
  f.bot.trial = f.features[sb::Feature::trial];
  // A change of course read a reaction late (`eye`), and the aim from its eye ahead (`selfaim`).
  f.bot.senses.set.course_s = f.features[sb::Feature::eye] ? f.bot.aim.react_med : 0;
  f.bot.selfaim = f.features[sb::Feature::selfaim];
  f.bot.corner = f.features[sb::Feature::corner];
  f.bot.glance = f.features[sb::Feature::glance];
  f.bot.lookway = f.features[sb::Feature::lookway];
  f.bot.calm_vel = f.features[sb::Feature::calmvel];
  f.bot.look_turn = f.features[sb::Feature::lookturn];
  f.bot.view_booked = f.features[sb::Feature::viewbook];
  f.bot.listen = f.features[sb::Feature::listen];
  f.bot.pools = f.bot.pilot.pilot.set.floor_swim = f.features[sb::Feature::pools];
}

}  // namespace shooters_edge::bots

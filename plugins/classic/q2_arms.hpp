// QUAKE II'S WEAPONS, AS ANY RULES FIRE THEM: the base game's eleven weapons as the hand steps
// them and the shots they make, in one place, so Quake II's own rules and another game's rules
// wearing Quake II's weapons (classic remix's `cr_items q2` on Quake's rules) fire the same
// weapons by the same code: the frames, the refire, the held fire, the damage, the spreads, the
// speeds and the blasts. Header-only and plain, so a rules package that links nothing but the
// program interface can include it.
//
// - **A weapon** (`Q2Arm`, `kQ2Arms` by `Q2ArmId`): its name, the ammunition a shot takes, the
//   last frame of each run (activate 0..a, fire a+1..f, idle f+1..i, deactivate i+1..d), the
//   frames that fire, a hit's damage in deathmatch and outside it, its knockback, and where the
//   shot starts from the eye (forward, right).
// - **The hand** steps once a rules frame (0.1 s). Raising (`q2_raise_step`) climbs the activate
//   run, `swap` frames a step, and is ready on the step after its last frame. Ready and idle
//   (`q2_idle_step`) loops the idle run, resting on its pause frames (`kQ2ArmPauses`). Firing (`q2_firing_step`) walks the fire run, a shot on
//   each fire frame, and is ready again a step after its last fire frame, so a weapon's period is
//   (last fire − last activate + 1) steps. Held fire (`q2_fire_frame`): the machinegun alternates
//   frames 4 and 5 while attack is held; the chaingun runs 5 to 21 (1, then 2 from frame 10, then
//   3 bullets a step from 15), loops 15 to 21 while held and stocked, and ends early when let go at
//   14; the hyperblaster loops 12 back to 6. The hand grenade's own step is its rules'.
// - **The hand's sounds** (`q2_arm_sound`): the grenade's pin pulled, the chaingun's spin up from
//   its first fire frame and down at the burst's end (not when let go early), the hyperblaster's
//   wind-down.
// - **The shots** (`kQ2Shot*`): a line's reach; the blaster's bolt; the shotgun's 12 pellets and
//   the super shotgun's two halves of 10 turned 5 degrees each way; the bullets' spread; the
//   chaingun's muzzle; the grenade's throw, fuse and blast; the rocket's speed, its hit's extra and
//   its blast; the rail's knockback; the BFG's ball. A spread is units across at the line's reach.
// - **A blow's push** (`q2_push`): 500 × knockback / mass, 1600 × on oneself (the rocket jump).
//
// ge:summary Quake II's weapons as any rules fire them: frames, hand step, shots, blasts, push
// ge:keywords q2 weapons q2-arms hand-step weapon-frames held-fire chaingun-spin machinegun
// ge:keywords hyperblaster railgun rail-kick pellets bullet-spread rocket grenade bfg-ball blast
// ge:keywords knockback remix-weapons cr_items real-weapons q2_firing_step q2_raise_step
// ge:keywords idle-pause pause-frames view-kick kick-angles kick-origin recoil
// ge:skill classic shooter
// ge:test tests/classic_q2_arms_test.cpp
#pragma once

#include <cstdint>

namespace ge::classic {

// The base game's weapons, in the order a number key picks them (1 the blaster); the hand
// grenade, last, is chosen by `use Grenades`.
enum class Q2ArmId : uint8_t {
  blaster,
  shotgun,
  super_shotgun,
  machinegun,
  chaingun,
  grenade_launcher,
  rocket_launcher,
  hyperblaster,
  railgun,
  bfg,
  hand_grenade,
};
inline constexpr int kQ2ArmCount = 11;

// A weapon's runs of frames: the last frame of each (activate 0..a, fire a+1..f, idle f+1..i,
// deactivate i+1..d) and the frames that fire (a held-fire weapon loops in them).
struct Q2ArmFrames {
  int last_activate, last_fire, last_idle, last_deactivate;
  int fire_from, fire_to;
};

// How a weapon's fire frames fire: once a frame, or one of the three held-fire runs; `loop` is
// another game's weapon fired by this hand (classic remix): a shot each step while attack is held
// and stocked, the fire frames round in turn.
enum class Q2Held : uint8_t { once, machinegun, chaingun, hyperblaster, loop };

struct Q2Arm {
  const char* name;  // the name a player reads and `use` takes
  int per_shot;      // ammunition a shot takes (0: the blaster's none)
  Q2ArmFrames frames;
  int damage_dm, damage_sp;  // a hit's damage, in deathmatch and outside it
  int kick;                  // a hit's knockback (the rail's own below)
  float forward, right;      // the shot's start from the eye, units
  Q2Held held = Q2Held::once;
};

inline constexpr Q2Arm kQ2Arms[kQ2ArmCount] = {
    {"Blaster", 0, {4, 8, 52, 55, 5, 5}, 15, 10, 1, 24, 8},
    {"Shotgun", 1, {7, 18, 36, 39, 8, 8}, 4, 4, 8, 8, 8},
    {"Super Shotgun", 2, {6, 17, 57, 61, 7, 7}, 6, 6, 12, 8, 8},
    {"Machinegun", 1, {3, 5, 45, 49, 4, 5}, 8, 8, 2, 8, 8, Q2Held::machinegun},
    {"Chaingun", 1, {4, 31, 61, 64, 5, 21}, 6, 8, 2, 8, 7, Q2Held::chaingun},
    {"Grenade Launcher", 1, {5, 16, 59, 64, 6, 6}, 120, 120, 0, 8, 8},
    {"Rocket Launcher", 1, {4, 12, 50, 54, 5, 5}, 100, 100, 0, 8, 8},
    {"HyperBlaster", 1, {5, 20, 49, 53, 6, 11}, 15, 20, 1, 24, 8, Q2Held::hyperblaster},
    {"Railgun", 1, {3, 18, 56, 61, 4, 4}, 100, 150, 200, 0, 7},
    {"BFG10K", 50, {8, 32, 55, 58, 17, 17}, 200, 500, 0, 8, 8},
    {"Grenades", 1, {0, 15, 48, 48, 11, 12}, 125, 125, 0, 8, 8},
};

// The word a rules program's arsenal names each weapon's fire by (its `fire` column), in `Q2ArmId`
// order.
inline constexpr const char* kQ2ArmFireWords[kQ2ArmCount] = {
    "q2_blaster",          "q2_shotgun",         "q2_super_shotgun", "q2_machinegun", "q2_chaingun",
    "q2_grenade_launcher", "q2_rocket_launcher", "q2_hyperblaster",  "q2_railgun",    "q2_bfg",
    "q2_hand_grenade"};

constexpr const Q2Arm& q2_arm(Q2ArmId id) { return kQ2Arms[static_cast<int>(id)]; }

// The hand's raise for a step, `swap` frames at a time (at least 1): true when it is ready (its
// frame then the first idle frame).
constexpr bool q2_raise_step(const Q2ArmFrames& a, int& frame, int swap = 1) {
  if (frame >= a.last_activate) {
    frame = a.last_fire + 1;
    return true;
  }
  const int to = frame + (swap > 1 ? swap : 1);
  frame = to < a.last_activate ? to : a.last_activate;
  return false;
}

// THE IDLE RUN'S PAUSES: the idle frames a ready hand may rest on (0: no more), by `Q2ArmId`. On a
// pause frame the hand stays a step with chance 15/16, so a rest lasts 1.6 s on average (1.1 s
// the median). The last idle frame goes back to the first before its pause is asked, so a pause
// listed there never holds, except the hand grenade's (`last_holds`): it rests on 48 too, then
// goes back to 16. The hyperblaster never rests.
struct Q2ArmPauses {
  int at[4];
  bool last_holds = false;
};
inline constexpr Q2ArmPauses kQ2ArmPauses[kQ2ArmCount] = {
    {{19, 32, 0, 0}},  {{22, 28, 34, 0}},  {{29, 42, 57, 0}}, {{23, 45, 0, 0}},
    {{38, 43, 51, 61}}, {{34, 51, 59, 0}}, {{25, 33, 42, 50}}, {{0, 0, 0, 0}},
    {{56, 0, 0, 0}},   {{39, 45, 50, 55}}, {{29, 34, 39, 48}, true},
};
inline constexpr Q2ArmPauses kQ2NoPauses{};

constexpr bool q2_pauses_on(const Q2ArmPauses& p, int frame) {
  for (const int at : p.at)
    if (at != 0 && at == frame) return true;
  return false;
}

// The draw a pause takes, from the step and the player (not the rules' random stream, so a rest
// moves no shot's spread): stays unless its low four bits are 0.
constexpr uint32_t q2_pause_draw(uint32_t step, uint32_t who) {
  uint32_t x = step * 0x9E3779B1u ^ (who + 0x7F4A7C15u) * 0x85EBCA77u;
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}
// A draw that always rests: a hand predicted ahead of its server waits there for the server's.
inline constexpr uint32_t kQ2PauseRests = 1;

// A ready hand with attack up: the idle run round and round, resting on its pause frames by
// `draw` (`q2_pause_draw`).
constexpr void q2_idle_step(const Q2ArmFrames& a, const Q2ArmPauses& p, int& frame,
                            uint32_t draw) {
  const bool rest = q2_pauses_on(p, frame) && (draw & 15u) != 0;
  if (frame >= a.last_idle && !(p.last_holds && rest)) {
    frame = a.last_fire + 1;
    return;
  }
  if (!rest) frame++;
}

// The shots of a fire frame (the held-fire runs), the frame moved on. `have`: the shots the
// ammunition holds; `held_shots` counts the machinegun's shots in a row (its climb).
constexpr int q2_fire_frame(const Q2ArmFrames& a, Q2Held held, int& frame, int& held_shots,
                            bool attack, int have) {
  const int idle = a.last_fire + 1;
  switch (held) {
    case Q2Held::machinegun:  // 4 and 5 in turn while held and stocked
      if (!attack || have < 1) {
        frame = idle;
        held_shots = 0;
        return 0;
      }
      frame = frame == 4 ? 5 : 4;
      held_shots++;
      return 1;
    case Q2Held::chaingun: {
      if (frame == 14 && !attack) {  // let go before the spin: it ends here
        frame = idle;
        return 0;
      }
      const int want = frame <= 9 ? 1 : frame <= 14 ? 2 : 3;
      const int shots = want < have ? want : have;
      if (shots == 0) {
        frame = idle;
        return 0;
      }
      frame = frame == 21 ? (attack && have > shots ? 15 : 22) : frame + 1;
      return shots;
    }
    case Q2Held::hyperblaster:
      if (!attack || have < 1) {
        frame = 12;
        return 0;
      }
      frame++;
      return 1;
    case Q2Held::loop:
      if (!attack || have < 1) {
        frame = idle;
        held_shots = 0;
        return 0;
      }
      frame = frame >= a.fire_to ? a.fire_from : frame + 1;
      held_shots++;
      return 1;
    default:
      frame++;
      return have >= 1 ? 1 : 0;
  }
}

// A firing hand's step: its shots, or `done` (ready again: the step after its last fire frame).
// `again`: the hyperblaster at the end of its loop goes round once more (attack held and a shot
// stocked).
struct Q2Firing {
  int shots = 0;
  bool done = false;
};
constexpr Q2Firing q2_firing_step(const Q2ArmFrames& a, Q2Held held, int& frame, int& held_shots,
                                  bool attack, int have, bool again) {
  Q2Firing out;
  if (frame > a.last_fire) {
    out.done = true;
    return out;
  }
  if (held == Q2Held::hyperblaster && frame == 12 && again) {
    frame = 6;  // held: round again, firing at once
    out.shots = q2_fire_frame(a, held, frame, held_shots, attack, have);
  } else if (frame >= a.fire_from && frame <= a.fire_to) {
    out.shots = q2_fire_frame(a, held, frame, held_shots, attack, have);
  } else {
    frame++;
  }
  return out;
}

// THE HAND'S ONE-SHOT SOUNDS, by `kQ2ArmSounds`'s row: the step that took weapon `id`'s hand from
// frame `was` (firing or not) to `frame` (firing or not); -1 none. A hand whose weapon changed
// in the step makes none (the caller's to tell).
inline constexpr const char* kQ2ArmSounds[] = {"weapons/hgrena1b.wav", "weapons/chngnu1a.wav",
                                               "weapons/chngnd1a.wav", "weapons/hyprbd1a.wav"};
constexpr int q2_arm_sound(Q2ArmId id, int was, bool was_firing, int frame, bool firing) {
  if (frame == was && was_firing) return -1;
  switch (id) {
    case Q2ArmId::hand_grenade:  // the pin pulled
      return frame == 5 ? 0 : -1;
    case Q2ArmId::chaingun:  // spun up from its first fire frame; down at the burst's end
      if (!was_firing && firing) return 1;
      return frame == 22 ? 2 : -1;
    case Q2ArmId::hyperblaster:  // wound down
      return frame == 12 ? 3 : -1;
    default:
      return -1;
  }
}

// THE SHOT'S SOUNDS, as a player hears another's gun (its muzzle flash's): each sample (a "%d"
// in it is one of 1..`variants`, drawn), `delay` s after the shot. The chaingun's two or three
// bullets a frame are heard apart (0.05 s; 0.033 and 0.066 s); the shotgun, the launchers and the
// railgun are heard working the action after the shot. Their number into `out`.
struct Q2ArmSound {
  const char* name = nullptr;
  int variants = 0;
  float delay = 0;
};
// The bullets' five samples, the "%d" of their name filled in, for rules that name each sample.
inline constexpr const char* kQ2GunSounds[5] = {"weapons/machgf1b.wav", "weapons/machgf2b.wav",
                                                "weapons/machgf3b.wav", "weapons/machgf4b.wav",
                                                "weapons/machgf5b.wav"};
constexpr int q2_arm_shot_sounds(Q2ArmId id, int shots, Q2ArmSound (&out)[3]) {
  constexpr Q2ArmSound kGun{"weapons/machgf%db.wav", 5, 0};
  switch (id) {
    case Q2ArmId::blaster:
      out[0] = {"weapons/blastf1a.wav"};
      return 1;
    case Q2ArmId::shotgun:
      out[0] = {"weapons/shotgf1b.wav"};
      out[1] = {"weapons/shotgr1b.wav", 0, 0.1f};
      return 2;
    case Q2ArmId::super_shotgun:
      out[0] = {"weapons/sshotf1b.wav"};
      return 1;
    case Q2ArmId::machinegun:
      out[0] = kGun;
      return 1;
    case Q2ArmId::chaingun:
      out[0] = kGun;
      if (shots == 2) {
        out[1] = {kGun.name, 5, 0.05f};
        return 2;
      }
      if (shots >= 3) {
        out[1] = {kGun.name, 5, 0.033f};
        out[2] = {kGun.name, 5, 0.066f};
        return 3;
      }
      return 1;
    case Q2ArmId::grenade_launcher:
      out[0] = {"weapons/grenlf1a.wav"};
      out[1] = {"weapons/grenlr1b.wav", 0, 0.1f};
      return 2;
    case Q2ArmId::rocket_launcher:
      out[0] = {"weapons/rocklf1a.wav"};
      out[1] = {"weapons/rocklr1b.wav", 0, 0.1f};
      return 2;
    case Q2ArmId::hyperblaster:
      out[0] = {"weapons/hyprbf1a.wav"};
      return 1;
    case Q2ArmId::railgun:
      out[0] = {"weapons/railgf1a.wav"};
      out[1] = {"weapons/railgr1b.wav", 0, 0.4f};
      return 2;
    case Q2ArmId::bfg:
      out[0] = {"weapons/bfg__f1y.wav"};
      return 1;
    case Q2ArmId::hand_grenade:
      out[0] = {"weapons/hgrent1a.wav"};
      return 1;
  }
  return 0;
}

// What the shots leave and make heard: a bolt's hit, a pellet's and a bullet's on a wall, blood,
// the rail's line, the blasts (in water their own), by the names the window draws them by; the
// blasts' and a grenade's bounce's sounds.
inline constexpr const char* kQ2HitBolt = "te_blaster";
inline constexpr const char* kQ2HitPellet = "te_shotgun";
inline constexpr const char* kQ2HitBullet = "te_gunshot";
inline constexpr const char* kQ2HitBlood = "te_blood";
inline constexpr const char* kQ2RailLine = "te_railtrail";
inline constexpr const char* kQ2RocketBlast = "te_rocket_explosion";
inline constexpr const char* kQ2GrenadeBlast = "te_grenade_explosion";
inline constexpr const char* kQ2RocketBlastWater = "te_rocket_explosion_water";
inline constexpr const char* kQ2GrenadeBlastWater = "te_grenade_explosion_water";
inline constexpr const char* kQ2RocketBoom = "weapons/rocklx1a.wav";
inline constexpr const char* kQ2GrenadeBoom = "weapons/grenlx1a.wav";
inline constexpr const char* kQ2GrenadeBounce = "weapons/grenlb1b.wav";
inline constexpr const char* kQ2NoAmmo = "weapons/noammo.wav";

// THE OBITUARY: what a kill by weapon `id` reads ("<victim> <words> <killer><tail>"), a blow
// landing straight on or as a blast's splash (a launcher's, the hand grenade's); the BFG's by how
// it landed (straight on, its laser in flight, its burst) is its rules' own. Null words: the
// weapon has none of its own here.
struct Q2ArmWords {
  const char* words = nullptr;
  const char* tail = "";
};
constexpr Q2ArmWords q2_arm_words(Q2ArmId id, bool blast) {
  switch (id) {
    case Q2ArmId::blaster:
      return {"was blasted by"};
    case Q2ArmId::shotgun:
      return {"was gunned down by"};
    case Q2ArmId::super_shotgun:
      return {"was blown away by", "'s super shotgun"};
    case Q2ArmId::machinegun:
      return {"was machinegunned by"};
    case Q2ArmId::chaingun:
      return {"was cut in half by", "'s chaingun"};
    case Q2ArmId::grenade_launcher:
      return blast ? Q2ArmWords{"was shredded by", "'s shrapnel"}
                   : Q2ArmWords{"was popped by", "'s grenade"};
    case Q2ArmId::rocket_launcher:
      return blast ? Q2ArmWords{"almost dodged", "'s rocket"} : Q2ArmWords{"ate", "'s rocket"};
    case Q2ArmId::hyperblaster:
      return {"was melted by", "'s hyperblaster"};
    case Q2ArmId::railgun:
      return {"was railed by"};
    case Q2ArmId::hand_grenade:
      return blast ? Q2ArmWords{"didn't see", "'s handgrenade"}
                   : Q2ArmWords{"caught", "'s handgrenade"};
    default:
      return {};
  }
}

// THE SHOTS.
inline constexpr float kQ2ShotRange = 8192;  // a line shot's reach
// The blaster's and the hyperblaster's bolt.
inline constexpr float kQ2BoltSpeed = 1000;
inline constexpr float kQ2BoltLife = 2;  // s
// Pellets and bullets: how many and their spread (units across and up at the line's reach).
inline constexpr int kQ2ShotgunPellets = 12;
inline constexpr float kQ2ShotgunSpreadH = 500, kQ2ShotgunSpreadV = 500;
inline constexpr int kQ2SuperHalfPellets = 10;  // each half's
inline constexpr float kQ2SuperTurn = 5;        // degrees of yaw each half turns, either way
inline constexpr float kQ2SuperSpreadH = 1000, kQ2SuperSpreadV = 500;
inline constexpr float kQ2BulletSpreadH = 300, kQ2BulletSpreadV = 500;
// The chaingun's bullets each start 7 ± 4 units right of the muzzle line and ± 4 up.
inline constexpr float kQ2ChainAcross = 7, kQ2ChainJitter = 4;
// The machinegun's climb outside deathmatch: degrees up for each bullet before it, at most 9 of
// them.
inline constexpr float kQ2MachineClimb = 1.5f;
inline constexpr int kQ2MachineClimbMost = 9;
// The grenade launcher's grenade: its speed, its lift and scatter (200 ± 10 up, ± 10 across),
// its fuse, its blast's reach (the hand grenade's a little more).
inline constexpr float kQ2GrenadeSpeed = 600;
inline constexpr float kQ2GrenadeLift = 200, kQ2GrenadeScatter = 10;
inline constexpr float kQ2GrenadeFuse = 2.5f;  // s
inline constexpr float kQ2GrenadeRadius = 160, kQ2HandGrenadeRadius = 165;
// The rocket: its speed and life (its reach over its speed), the extra its hit takes (0..20),
// its blast's damage and reach.
inline constexpr float kQ2RocketSpeed = 650;
inline constexpr float kQ2RocketLife = 8000.0f / 650.0f;  // s
inline constexpr int kQ2RocketExtra = 20;
inline constexpr int kQ2RocketSplash = 120;
inline constexpr float kQ2RocketRadius = 120;
// The rail's knockback, in deathmatch and outside it.
inline constexpr int kQ2RailKickDm = 200, kQ2RailKickSp = 250;
// The BFG's ball: its speed, its blast, the reach of its lasers in flight and their blow.
inline constexpr float kQ2BfgSpeed = 400;
inline constexpr int kQ2BfgSplash = 200;
inline constexpr float kQ2BfgRadius = 100, kQ2BfgLaserReach = 256;
inline constexpr int kQ2BfgLaserDm = 5, kQ2BfgLaserSp = 10;

// THE VIEW'S KICK A SHOT GIVES, by `Q2ArmId`. Pitch is down-positive, so a kick of -1 tips the view
// 1 degree up; `back` moves the eye that many units back along the whole aim (its pitch too).
// `shake`: pitch, yaw and roll each ± `kQ2KickShake` degrees and the eye ± `kQ2KickShakeMove`
// units along each of the world's axes (the chaingun); `climb`: the same but its pitch is the
// machinegun's climb, never shaken; `heave`: the eye back and the view's hurt kick (pitch -40, roll
// ± 8, fading over 0.5 s) instead of a kick of its own (the BFG). The hand grenade kicks none.
enum class Q2KickKind : uint8_t { none, straight, shake, climb, heave };
struct Q2KickRow {
  Q2KickKind kind;
  float pitch, back;
};
inline constexpr Q2KickRow kQ2Kicks[kQ2ArmCount] = {
    {Q2KickKind::straight, -1, 2}, {Q2KickKind::straight, -2, 2}, {Q2KickKind::straight, -2, 2},
    {Q2KickKind::climb, 0, 0},     {Q2KickKind::shake, 0, 0},     {Q2KickKind::straight, -1, 2},
    {Q2KickKind::straight, -1, 2}, {Q2KickKind::straight, -1, 2}, {Q2KickKind::straight, -3, 3},
    {Q2KickKind::heave, 0, 2},     {Q2KickKind::none, 0, 0},
};
inline constexpr float kQ2KickShake = 0.7f, kQ2KickShakeMove = 0.35f;
inline constexpr float kQ2HeavePitch = -40, kQ2HeaveRoll = 8, kQ2HeaveFade = 0.5f;

// A kick as the view shows it: angles (pitch, yaw, roll, degrees) and the eye's move (world
// x, y, z, units).
struct Q2Kick {
  float angles[3] = {0, 0, 0};
  float origin[3] = {0, 0, 0};
};

// A uniform draw in -most..most from a hash's state (stepped).
constexpr float q2_kick_jitter(uint32_t& state, float most) {
  state = q2_pause_draw(state, 0x6b1du);
  const float u = static_cast<float>(state >> 8) * (1.0f / 16777216.0f);  // 0..1
  return (u * 2 - 1) * most;
}

// The wire's grain: quarter units, cut toward zero, held to a signed byte (-32..31.75).
constexpr float q2_kick_wire(float v) {
  const float q = static_cast<float>(static_cast<int>(v * 4));
  return (q < -128 ? -128 : q > 127 ? 127 : q) * 0.25f;
}

// The kick weapon `id`'s shot gives the view looking along `forward` (a unit vector): `climb`
// is the machinegun's pitch up (degrees, ≥ 0; 0 in deathmatch), `draw` seeds the shakes. As the
// view receives it, on the wire's grain. The BFG's hurt kick is the view's own (`kQ2Heave*`).
constexpr Q2Kick q2_shot_kick(Q2ArmId id, const float forward[3], float climb, uint32_t draw) {
  const Q2KickRow& r = kQ2Kicks[static_cast<int>(id)];
  Q2Kick k;
  switch (r.kind) {
    case Q2KickKind::straight:
    case Q2KickKind::heave:
      k.angles[0] = r.pitch;
      for (int i = 0; i < 3; i++) k.origin[i] = -forward[i] * r.back;
      break;
    case Q2KickKind::shake:
    case Q2KickKind::climb: {
      uint32_t st = draw;
      const float pitch = q2_kick_jitter(st, kQ2KickShake);
      k.angles[0] = r.kind == Q2KickKind::climb ? -climb : pitch;
      k.angles[1] = q2_kick_jitter(st, kQ2KickShake);
      k.angles[2] = q2_kick_jitter(st, kQ2KickShake);
      for (float& o : k.origin) o = q2_kick_jitter(st, kQ2KickShakeMove);
      break;
    }
    case Q2KickKind::none:
      break;
  }
  for (int i = 0; i < 3; i++) {
    k.angles[i] = q2_kick_wire(k.angles[i]);
    k.origin[i] = q2_kick_wire(k.origin[i]);
  }
  return k;
}

// The machinegun's climb for its shot after `before` shots in a row: 1.5 degrees each, at most 9
// of them; none in deathmatch.
constexpr float q2_machine_climb(int before, bool deathmatch) {
  if (deathmatch || before <= 0) return 0;
  return kQ2MachineClimb * static_cast<float>(before < kQ2MachineClimbMost ? before
                                                                          : kQ2MachineClimbMost);
}

// THE KICK DRAWN: a kick lives in one of the rules' frames (0.1 s) and the view eases from the
// frame before to it, so it rises over 0.1 s and falls back over the next (a triangle 0.2 s wide);
// a weapon kicking every frame eases from one kick to the next.
inline constexpr double kQ2KickFrameS = 0.1;
struct Q2KickBlend {
  Q2Kick prev, cur;
  double at = -1;  // when `cur`'s frame began, s; -1 none
};
constexpr Q2Kick q2_kick_at(const Q2KickBlend& b, double t) {
  Q2Kick out;
  if (b.at < 0) return out;
  double f = (t - b.at) / kQ2KickFrameS;
  f = f < 0 ? 0 : f > 1 ? 1 : f;
  const auto g = static_cast<float>(f);
  for (int i = 0; i < 3; i++) {
    out.angles[i] = b.prev.angles[i] + (b.cur.angles[i] - b.prev.angles[i]) * g;
    out.origin[i] = b.prev.origin[i] + (b.cur.origin[i] - b.prev.origin[i]) * g;
  }
  return out;
}
// The frames up to `t`: each one with no shot eases back toward none.
constexpr void q2_kick_advance(Q2KickBlend& b, double t) {
  for (int n = 0; b.at >= 0 && t >= b.at + kQ2KickFrameS; n++) {
    if (n >= 2) {  // a long gap: nothing left to ease
      b = Q2KickBlend{};
      return;
    }
    b.prev = b.cur;
    b.cur = Q2Kick{};
    b.at += kQ2KickFrameS;
  }
}
// A shot's kick in the frame that begins at `t`.
constexpr void q2_kick_shot(Q2KickBlend& b, double t, const Q2Kick& k) {
  q2_kick_advance(b, t);
  b.prev = q2_kick_at(b, t);
  b.cur = k;
  b.at = t;
}

// Whether the hand's step from frame `was` to `now` fired (read off the frames alone, as a window
// that sees only the frame does): the machinegun on each change between its two fire frames; any
// other on a step that leaves a fire frame for the next frame (from the frame before it, from
// outside the fire run as it starts, or round its loop). The hyperblaster let go on its last fire
// frame reads as one shot more.
constexpr bool q2_frame_fired(const Q2ArmFrames& a, Q2Held held, int was, int now) {
  if (now == was) return false;
  if (held == Q2Held::machinegun) return now >= a.fire_from && now <= a.fire_to;
  const int left = now - 1;
  if (left < a.fire_from || left > a.fire_to) return false;
  const bool in_run = was > a.last_activate && was <= a.last_fire;
  return was == left || !in_run || now < was;
}

// A blow's push along its way: 500 × knockback / mass (at least 50), 1600 × when a player hits
// themselves (the rocket jump). A player's mass is 200.
inline constexpr float kQ2PlayerMass = 200;
constexpr float q2_push(int knockback, float mass, bool self) {
  const float m = mass < 50 ? 50 : mass;
  return (self ? 1600.0f : 500.0f) * static_cast<float>(knockback) / m;
}

}  // namespace ge::classic

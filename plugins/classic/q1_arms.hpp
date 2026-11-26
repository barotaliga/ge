// QUAKE'S WEAPONS, AS ANY RULES FIRE THEM: the numbers of Quake's eight weapons that live in their
// shots rather than in a weapon's row (`profiles::kQ1Weapons` holds the cost, the period, the
// damage, the reach and the names), in one place, so Quake's own rules and another game's rules
// wearing Quake's weapons (classic remix's `cr_items q1` on Quake II's rules) fire the same
// weapons by the same numbers. Header-only and plain, so a rules package that links nothing but
// the program interface can include it.
//
// - **The shots**: the axe's cut lands 0.2 s after the swing, 64 units from 16 over the origin;
//   the shotgun's 6 pellets and the super shotgun's 14, 4 points each, each pellet turned by
//   crandom × the spread across and up; the nailgun's barrels 4 units right and left in turn, its
//   spike from 16 over the origin; the grenade thrown at 600 forward and 200 up (aimed level: 600
//   and straight 200 up), scattered by crandom × 10 each way, spinning, bursting on what it
//   touches that bleeds; the rocket from 8 forward and 16 up, its hit 100 + 0..20; the lightning
//   gun's beam from 16 over the origin, 600 long, 30 to each thing on three lines; under water it
//   lets every cell go at once, 35 a cell round the firer.
// - **The hand** (`Q1ArmFrames`): Quake's view models stand still on frame 0 and play their fire
//   run once a shot (the nailguns and the lightning gun loop theirs while held), at 10 frames a
//   second; a weapon is ready again when its period is up, whatever frame it shows.
// - **The hits' looks** (`kQ1Hit*`): the names the window draws Quake's own effects by.
//
// ge:summary Quake's weapons as any rules fire them: pellets, spikes, grenade, rocket, beam, frames
// ge:keywords q1 weapons q1-arms quake-weapons pellets spike nail-side grenade-throw rocket-hit
// ge:keywords lightning-beam discharge axe-cut view-frames remix-weapons cr_items real-weapons
// ge:skill classic shooter
// ge:test tests/shooters_edge_remix_q1_arms_test.cpp
#pragma once

#include <cstdint>

namespace ge::classic {

// Quake's weapons, in the order a number key picks them (1 the axe).
enum class Q1ArmId : uint8_t {
  axe,
  shotgun,
  super_shotgun,
  nailgun,
  super_nailgun,
  grenade_launcher,
  rocket_launcher,
  lightning,
};
inline constexpr int kQ1ArmCount = 8;

// The axe: its cut lands this long after the swing, this far from 16 over the origin.
inline constexpr float kQ1AxeDelay = 0.2f;
inline constexpr float kQ1AxeReach = 64;
inline constexpr float kQ1AxeDamage = 20;
inline constexpr float kQ1EyeLift =
    16;  // the axe's, the spikes', the rocket's and the beam's start

// The shotguns: pellets, each pellet's points, and the spread across and up (a share of the
// forward line), out to 2048 units.
inline constexpr int kQ1ShotgunPellets = 6;
inline constexpr float kQ1ShotgunAcross = 0.04f, kQ1ShotgunUp = 0.04f;
inline constexpr int kQ1SuperPellets = 14;
inline constexpr float kQ1SuperAcross = 0.14f, kQ1SuperUp = 0.08f;
inline constexpr float kQ1PelletDamage = 4;
inline constexpr float kQ1PelletReach = 2048;

// The nailguns: the spike's and the super spike's points, their speed, and the nailgun's barrels
// either side (right first).
inline constexpr float kQ1SpikeDamage = 9, kQ1SuperSpikeDamage = 18;
inline constexpr float kQ1SpikeSpeed = 1000;
inline constexpr float kQ1SpikeLife = 6;
inline constexpr float kQ1NailSide = 4;

// The grenade: thrown forward and lifted, scattered, its fuse, its blast.
inline constexpr float kQ1GrenadeSpeed = 600, kQ1GrenadeLift = 200, kQ1GrenadeScatter = 10;
inline constexpr float kQ1GrenadeFuse = 2.5f;
inline constexpr float kQ1GrenadeSplash = 120;

// The rocket: from 8 forward and 16 up, its speed, its hit (100 + 0..20) and its blast.
inline constexpr float kQ1RocketForward = 8;
inline constexpr float kQ1RocketSpeed = 1000;
inline constexpr float kQ1RocketLife = 5;
inline constexpr float kQ1RocketHit = 100, kQ1RocketHitExtra = 20;
inline constexpr float kQ1RocketSplash = 120;

// The lightning gun: its reach, each line's points, the loud hit's sound at most this often, and
// under water each cell's share of the discharge.
inline constexpr float kQ1BeamReach = 600;
inline constexpr float kQ1BeamDamage = 30;
inline constexpr float kQ1BeamSoundEvery = 0.6f;
inline constexpr float kQ1DischargePerCell = 35;

// A Quake view model's fire run, by weapon: its first and last fire frames (frame 0 stands
// still). A held weapon (the nailguns, the lightning gun) loops the run while it fires.
struct Q1ArmFrames {
  int first, last;
  bool loops;
};
inline constexpr Q1ArmFrames kQ1ArmFrames[kQ1ArmCount] = {
    {1, 4, false},  // the axe's swing
    {1, 6, false},  // the shotgun
    {1, 6, false},  // the super shotgun
    {1, 8, true},   // the nailgun
    {1, 8, true},   // the super nailgun
    {1, 6, false},  // the grenade launcher
    {1, 6, false},  // the rocket launcher
    {1, 4, true},   // the lightning gun
};

// The frame a Quake view model shows `steps` rules frames (0.1 s each) into its fire run, 0 once a
// run that does not loop is over.
constexpr int q1_view_frame(Q1ArmId id, int steps) {
  const Q1ArmFrames& f = kQ1ArmFrames[static_cast<int>(id)];
  const int length = f.last - f.first + 1;
  if (steps < 0) return 0;
  if (f.loops) return f.first + steps % length;
  return steps < length ? f.first + steps : 0;
}

// What Quake's shots leave, by the names the window draws Quake's own looks by in any game (a
// game's own `te_` names would take that game's look): a pellet's puff on a wall, blood, a
// spike's and a super spike's mark, the blasts, the beam.
inline constexpr const char* kQ1HitPellet = "q1_gunshot";
inline constexpr const char* kQ1HitBlood = "q1_blood";
inline constexpr const char* kQ1HitSpike = "q1_spike";
inline constexpr const char* kQ1HitSuperSpike = "q1_superspike";
inline constexpr const char* kQ1Blast = "q1_explosion";
inline constexpr const char* kQ1Beam = "q1_lightning2";
// The sounds the shots make beyond each row's own: the beam's start and its loud hit, the blasts,
// a grenade's bounce.
inline constexpr const char* kQ1BeamStart = "weapons/lstart.wav";
inline constexpr const char* kQ1BeamHit = "weapons/lhit.wav";
inline constexpr const char* kQ1Blasted = "weapons/r_exp3.wav";
inline constexpr const char* kQ1Bounce = "weapons/bounce.wav";

// A Quake look's name without its `q1_` (the window draws "q1_spike" as Quake's "te_spike"), or
// null for any other name.
constexpr const char* q1_look_name(const char* name) {
  if (name == nullptr || name[0] != 'q' || name[1] != '1' || name[2] != '_') return nullptr;
  return name + 3;
}

}  // namespace ge::classic

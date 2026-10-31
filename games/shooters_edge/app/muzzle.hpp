// SHOOTER'S EDGE'S MUZZLE FIRE: each shot of the weapon in hand fires the engine's muzzle fire
// (render/muzzle.hpp, drawn by the explosions' pass, render/explosions.hpp): a flame on the
// barrel's tip that rides the gun's kick and its firing animation, a puff of smoke left in the
// world where the shot went off, and a few sparks flung down the barrel's line into the particles.
// Every weapon's fire is its table row's (view/weapons.hpp: a preset and its knobs at the true
// size), scaled by the share of its true size the gun is drawn at, so the flame fits the gun.
//
// - `fire_looks` turns the table's rows into looks once (the shots point at them: they live as
//   long as the window's set).
// - `step_muzzle` each display frame: a shot fired starts its fire on the muzzle's pose of that
//   frame (its sparks thrown then, its puff born there), and every live shot hands the world pass
//   its flame on THIS frame's pose and its puff, until the puff has thinned away.
//
// Each shot draws its noise and its sparks off its own seed from a counter, so a replay throws the
// same fire.
//
// ge:summary Shooter's Edge's muzzle fire: the weapons' looks, the shots' flames, smoke and sparks
// ge:keywords shooters-edge muzzle muzzle-fire muzzle-flash flame smoke puff sparks shot
// ge:keywords fire_looks step_muzzle muzzle-shots first-person explosions-pass
// ge:skill render shooter
// ge:test tests/shooters_edge_muzzle_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "render/explosion.hpp"
#include "render/explosions.hpp"
#include "render/muzzle.hpp"
#include "render/particles.hpp"

namespace shooters_edge::effects {

namespace gr = ge::render;

// One weapon's fire as its table gives it: the preset and its knobs ("": none).
struct FireRow {
  std::string_view preset;
  std::string_view knobs;
};

// Every row's look scaled by `scale` (the share of its true size the gun is drawn at), in the
// rows' order; a row with no preset gets a look of no flame and no smoke and is never fired.
// False with the row and the reason in `error` when a preset or a knob is not known.
bool fire_looks(std::span<const FireRow> rows, double scale, std::vector<gr::MuzzleLook>& out,
                std::string* error);
// Whether a look made by fire_looks throws anything.
bool fires(const gr::MuzzleLook& look);

struct MuzzleShots {
  struct Live {
    gr::Shot shot;
    gr::MuzzlePose at_shot;  // the pose it went off on: its puff and its sparks start there
  };
  std::vector<Live> live;
  uint32_t seed = 1;
};

// One display frame at `now_s` (the world pass's clock): when `fired`, a shot of `look` (null or a
// look that throws nothing: none) starts on `pose` and its sparks go into `births`; then every
// live shot's flame on `pose` (this frame's muzzle) into `flames` while it burns and its puff into
// `puffs` until it has thinned away.
void step_muzzle(MuzzleShots& s, const gr::MuzzleLook* look, bool fired, const gr::MuzzlePose& pose,
                 double now_s, std::vector<gr::Particle>* births, std::vector<gr::Blast>* puffs,
                 std::vector<gr::MuzzleFire>* flames);

}  // namespace shooters_edge::effects

// EVERY ENHANCEMENT A SWITCH: what classic-enhance adds to the classic game, each thing on or off
// on its own, so a player keeps what they like and has the original for the rest. A switch off
// means the classic original, as the pack has it (the pack's picture, its light, its sound, its
// effect sprite).
//
// - **The catalog** (`kSwitches`) is the one list: a switch's id, its group, the words the menu
//   shows, its default, and whether it changes what the pipeline BAKES into the cache (the
//   textures' upscaler, the rounding of the models) or only what is DRAWN and PLAYED from it.
//   The menu's pages (menu.hpp), the cvars and the tests all read it.
// - **The values** (`Switches`) are one bool a switch, read by whatever draws or plays the thing
//   (the brush renderer, the effects, the sound) through the cvar bound to it.
// - **The cvars** are `ce_<id>` (`bind_switches`), saved with the player's settings, so a config
//   or the console flips one (`ce_decals 0`) the same as the menu does.
// - `apply_bakes` turns the bake switches into the pipeline's options, so a switch that changes
//   the cache reruns the stage it touches (the stage's key holds the options).
//
// ge:summary Every classic-enhance effect a switch: the catalog, the values, the ce_ cvars
// ge:keywords classicenhance switches settings toggle cvar ce_ decals flames skies water lava
// ge:keywords muzzle teleports rubble fog bloom reflections sounds models upscaler options menu
// ge:skill classicenhance
// ge:test tests/classicenhance_switches_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "classicenhance/pipeline.hpp"
#include "console/cvar.hpp"

namespace ge::classicenhance {

struct Switches {
  // Textures
  bool textures = true, upscaler = true, relief = true, glow = true;
  // Light
  bool rebake = true, flicker = true, shadows = true;
  // World
  bool arches = true, bevels = true, rubble = true, clutter = true, decals = true;
  // Liquids and sky
  bool water = true, lava = true, slime = true, teleports = true, skies = true;
  // Effects
  bool flames = true, muzzles = true, explosions = true, flares = true, bubbles = true,
       beams = true;
  // Air
  bool fog = true, clouds = true, wind = true, weather = true;
  // Picture
  bool reflections = true, bloom = true;
  // Models
  bool models = true, smooth = true, rounded = false, weapons = true;
  // Sound
  bool sounds = true, acoustics = true;
};

struct SwitchSpec {
  std::string_view id;     // the cvar is ce_<id>
  std::string_view group;  // the menu's page
  std::string_view text;   // the menu's words
  std::string_view hint;   // on: what it does; off: what the original shows
  bool Switches::*field;
  bool bake;  // changes the cache, not only the drawing
};

extern const SwitchSpec kSwitches[];
extern const size_t kSwitchCount;
std::span<const SwitchSpec> switches();

// The groups in the menu's order.
extern const std::string_view kSwitchGroups[];
extern const size_t kSwitchGroupCount;

// The switch with that id; null when none.
const SwitchSpec* find_switch(std::string_view id);

// Each switch as ce_<id> (boolean, archived), writing `values` on every change; the defaults are
// the catalog's (a default `Switches`).
void bind_switches(console::Cvars& cvars, Switches* values);

// The bake switches into the pipeline's options: the trained upscaler off leaves the kernel
// (`network` cleared), rounding on rounds the models (round 4) and off keeps them smooth.
void apply_bakes(const Switches& values, RunOptions* opts);

}  // namespace ge::classicenhance

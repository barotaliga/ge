// The switches' catalog and cvars (switches.hpp).
//
// ge:summary The classic-enhance switches' catalog, their cvars and their bake options
// ge:keywords classicenhance switches catalog cvar bind bake options
// ge:skill classicenhance
// ge:test tests/classicenhance_switches_test.cpp
#include "classicenhance/switches.hpp"

#include <string>

namespace ge::classicenhance {

using S = Switches;

const SwitchSpec kSwitches[] = {
    {"textures", "TEXTURES", "ENHANCED TEXTURES",
     "The walls and floors upscaled and given depth. Off: the pack's own pictures.", &S::textures,
     false},
    {"upscaler", "TEXTURES", "TRAINED UPSCALER",
     "The trained network upscales the pictures. Off: the edge-aware kernel (a faster bake).",
     &S::upscaler, true},
    {"relief", "TEXTURES", "SURFACE RELIEF",
     "Normal and roughness maps light the surfaces' bumps and shine. Off: flat surfaces.",
     &S::relief, false},
    {"parallax", "TEXTURES", "SURFACE DEPTH",
     "The height maps sink the mortar and cracks into the walls. Off: the bumps lie flat.",
     &S::parallax, false},
    {"glow", "TEXTURES", "GLOWING TEXELS",
     "The bright texels of lamps and signs glow. Off: they are just bright.", &S::glow, false},
    {"rebake", "LIGHT", "REBAKED LIGHT",
     "The light bounces off the walls, coloured by them, worked out again. Off: the pack's light.",
     &S::rebake, false},
    {"flicker", "LIGHT", "LIVE LIGHTS",
     "Torches and flames cast flickering light that moves shadows. Off: their light is baked.",
     &S::flicker, false},
    {"shadows", "LIGHT", "SHADOWS",
     "Live lights and the monsters cast shadows. Off: no cast shadows.", &S::shadows, false},
    {"arches", "WORLD", "ROUND ARCHES",
     "Faceted arches and pillars drawn round. Off: their facets as built.", &S::arches, false},
    {"bevels", "WORLD", "BEVELLED EDGES",
     "The brushes' exposed edges bevelled to catch the light. Off: sharp edges.", &S::bevels,
     false},
    {"rubble", "WORLD", "RUBBLE AND DEBRIS",
     "Stones and debris along walls and in corners. Off: bare floors.", &S::rubble, false},
    {"clutter", "WORLD", "FLOOR CLUTTER",
     "Moss, dust and litter by surface and open sky. Off: none.", &S::clutter, false},
    {"decals", "WORLD", "DECALS",
     "Scorch marks, bullet holes and blood that age on the walls. Off: no marks.", &S::decals,
     false},
    {"water", "LIQUIDS AND SKY", "WATER",
     "Water that reflects, refracts and ripples. Off: the pack's warped picture.", &S::water,
     false},
    {"lava", "LIQUIDS AND SKY", "LAVA",
     "Lava that glows, crusts and lights the room. Off: the pack's warped picture.", &S::lava,
     false},
    {"slime", "LIQUIDS AND SKY", "SLIME",
     "Slime that is thick and glows. Off: the pack's warped picture.", &S::slime, false},
    {"teleports", "LIQUIDS AND SKY", "TELEPORTERS",
     "Teleporters drawn as a shimmering field. Off: the pack's warped picture.", &S::teleports,
     false},
    {"skies", "LIQUIDS AND SKY", "SKIES",
     "A sky with depth, clouds and the evening light. Off: the pack's scrolling sky.", &S::skies,
     false},
    {"flames", "EFFECTS", "FIRE",
     "Torches and braziers burn as live fire. Off: the pack's painted flames.", &S::flames, false},
    {"bonfires", "EFFECTS", "BONFIRES",
     "Braziers burn as bonfires throwing sparks into the wind, lanterns brighter. Off: the "
     "pack's flame sizes.",
     &S::bonfires, false},
    {"muzzles", "EFFECTS", "MUZZLE FIRE",
     "Each shot's flash and smoke from the barrel. Off: the weapons' own flash.", &S::muzzles,
     false},
    {"explosions", "EFFECTS", "EXPLOSIONS",
     "Rockets and grenades explode in fire and smoke. Off: the pack's explosion sprite.",
     &S::explosions, false},
    {"flares", "EFFECTS", "FLARES", "Lights flare as glows. Off: the pack's flare sprite.",
     &S::flares, false},
    {"bubbles", "EFFECTS", "BUBBLES",
     "Bubbles that rise and wobble under water. Off: the pack's bubble sprite.", &S::bubbles,
     false},
    {"beams", "EFFECTS", "LIGHTNING BEAMS",
     "The lightning gun's bolt as a live, branching beam. Off: the pack's bolt models.", &S::beams,
     false},
    {"fog", "AIR", "FOG", "Fog and haze in the distance and in the rooms. Off: clear air.", &S::fog,
     false},
    {"clouds", "AIR", "LOW CLOUDS", "Low cloud over the open levels. Off: the sky alone.",
     &S::clouds, false},
    {"wind", "AIR", "WIND", "A wind that moves flames, smoke and loose things. Off: still air.",
     &S::wind, false},
    {"weather", "AIR", "WEATHER",
     "Rain, snow and the evening's weather over the open levels. Off: none.", &S::weather, false},
    {"reflections", "PICTURE", "REFLECTIONS",
     "Wet and glossy surfaces reflect the room. Off: no reflections.", &S::reflections, false},
    {"bloom", "PICTURE", "BLOOM", "Bright lights bleed a soft glow. Off: a hard picture.",
     &S::bloom, false},
    {"models", "MODELS", "ENHANCED SKINS",
     "The monsters' and weapons' skins upscaled. Off: the pack's skins.", &S::models, false},
    {"smooth", "MODELS", "SMOOTH SHADING",
     "The models shaded smooth across their facets. Off: flat facets.", &S::smooth, false},
    {"rounded", "MODELS", "ROUNDED MODELS",
     "The models' facets rounded into curved patches (swells thin parts). Off: as built.",
     &S::rounded, true},
    {"weapons", "MODELS", "LIT WEAPONS",
     "The held weapon lit by the room and its flashes. Off: the classic flat light.", &S::weapons,
     false},
    {"sounds", "SOUND", "ENHANCED SOUNDS",
     "Every sound cleaned of hiss and brought to 48 kHz. Off: the pack's sounds.", &S::sounds,
     false},
    {"acoustics", "SOUND", "ROOM ACOUSTICS", "Sounds echo by the room they are in. Off: dry sound.",
     &S::acoustics, false},
};
const size_t kSwitchCount = sizeof(kSwitches) / sizeof(kSwitches[0]);

const std::string_view kSwitchGroups[] = {"TEXTURES",        "LIGHT",   "WORLD",
                                          "LIQUIDS AND SKY", "EFFECTS", "AIR",
                                          "PICTURE",         "MODELS",  "SOUND"};
const size_t kSwitchGroupCount = sizeof(kSwitchGroups) / sizeof(kSwitchGroups[0]);

std::span<const SwitchSpec> switches() { return {kSwitches, kSwitchCount}; }

const SwitchSpec* find_switch(std::string_view id) {
  for (const SwitchSpec& s : switches()) {
    if (s.id == id) return &s;
  }
  return nullptr;
}

void bind_switches(console::Cvars& cvars, Switches* values) {
  static const Switches kDefaults;
  // The names and help must outlive the registry's copy of them only for this call.
  for (const SwitchSpec& s : switches()) {
    const std::string name = "ce_" + std::string(s.id);
    const std::string help = std::string(s.text) + ": " + std::string(s.hint);
    console::CvarSpec spec;
    spec.name = name;
    spec.fallback = kDefaults.*s.field ? "1" : "0";
    spec.flags = console::kArchive;
    spec.help = help;
    spec.kind = console::CvarKind::boolean;
    spec.min = 0;
    spec.max = 1;
    cvars.bind(spec, &(values->*s.field));
  }
}

void apply_bakes(const Switches& values, RunOptions* opts) {
  if (!values.upscaler) opts->textures.network = nullptr;
  opts->models.round = values.rounded ? 4 : 0;
}

}  // namespace ge::classicenhance

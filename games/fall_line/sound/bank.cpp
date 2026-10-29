// Fall Line's bank as data (bank.hpp): every sound a run makes, its sentence and its voices.
//
// ge:summary Fall Line's sound bank: the landings, the trunks, the gates, the finish, the falls
// ge:keywords bank sound-def landing tree gate missed finish wipeout reset bump clatter
// ge:skill audio sim
// ge:test tests/fall_line_sound_test.cpp
#include "fall_line/sound/bank.hpp"

namespace fall_line::sound {

namespace {

using ge::audio::Color;
using ge::audio::FilterType;
using ge::audio::noise_voice;
using ge::audio::SoundDef;
using ge::audio::SoundVoice;
using ge::audio::tone_voice;
using ge::audio::Wave;

// THE SKIS KNOCKING OFF ON THEIR OWN after a fall: two short driven triangle clacks, a beat
// apart, the second quieter and lower. Every wipeout ends on it, because a skier thrown is a body
// and two skis arriving separately.
constexpr SoundVoice clack(float at_ms, float volume, float from, float to, float band) {
  return tone_voice({.wave = Wave::triangle,
                     .from_hz = from,
                     .to_hz = to,
                     .duration_ms = 45,
                     .volume = volume,
                     .delay_ms = at_ms,
                     .filter = {.type = FilterType::bandpass, .hz = band, .q = 1.8f},
                     .drive = 0.5f});
}

// THE MENUS speak softly and dry, because they sit over the mountain's wind: a focus moved is
// a short wooden tick, a press a low knock with a brighter tick on it, a page a short pink
// breath of snow swept aside.
constexpr SoundVoice kMenuFocusVoices[] = {
    tone_voice({.wave = Wave::triangle,
                .from_hz = 1800,
                .to_hz = 1500,
                .duration_ms = 22,
                .volume = 0.05f,
                .filter = {.type = FilterType::bandpass, .hz = 1700, .q = 2.5f},
                .drive = 0.3f}),
};

constexpr SoundVoice kMenuPressVoices[] = {
    tone_voice({.wave = Wave::sine,
                .from_hz = 220,
                .to_hz = 150,
                .duration_ms = 70,
                .volume = 0.06f,
                .attack_ms = 2,
                .drive = 0.4f}),
    tone_voice({.wave = Wave::triangle,
                .from_hz = 2400,
                .to_hz = 2000,
                .duration_ms = 25,
                .volume = 0.025f,
                .filter = {.type = FilterType::bandpass, .hz = 2200, .q = 2.0f}}),
};

constexpr SoundVoice kMenuPageVoices[] = {
    noise_voice({.duration_ms = 160,
                 .volume = 0.07f,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 900, .to_hz = 2600, .q = 0.8f},
                 .attack_ms = 20}),
};

constexpr SoundVoice kLandSoftVoices[] = {
    tone_voice({.wave = Wave::sine,
                .from_hz = 90,
                .to_hz = 55,
                .duration_ms = 150,
                .volume = 0.05f,
                .attack_ms = 4,
                .drive = 0.3f}),
    noise_voice({.duration_ms = 220,
                 .volume = 0.04f,
                 .color = Color::brown,
                 .filter = {.type = FilterType::lowpass, .hz = 380, .to_hz = 180},
                 .attack_ms = 6}),
    noise_voice({.duration_ms = 320,
                 .volume = 0.026f,
                 .delay_ms = 20,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 1200, .to_hz = 3200, .q = 0.7f},
                 .attack_ms = 14}),
};

constexpr SoundVoice kLandHardVoices[] = {
    noise_voice({.duration_ms = 30,
                 .volume = 0.05f,
                 .filter = {.type = FilterType::bandpass, .hz = 700, .q = 1.6f}}),
    tone_voice({.wave = Wave::sine,
                .from_hz = 78,
                .to_hz = 42,
                .duration_ms = 240,
                .volume = 0.07f,
                .attack_ms = 2,
                .drive = 0.6f}),
    noise_voice({.duration_ms = 320,
                 .volume = 0.05f,
                 .color = Color::brown,
                 .filter = {.type = FilterType::lowpass, .hz = 320, .to_hz = 140},
                 .attack_ms = 5}),
    noise_voice({.duration_ms = 480,
                 .volume = 0.034f,
                 .delay_ms = 25,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 900, .to_hz = 2800, .q = 0.7f},
                 .attack_ms = 18,
                 .hold_ms = 60}),
};

constexpr SoundVoice kLandPowderVoices[] = {
    noise_voice({.duration_ms = 340,
                 .volume = 0.05f,
                 .color = Color::brown,
                 .filter = {.type = FilterType::lowpass, .hz = 420, .to_hz = 160},
                 .attack_ms = 14,
                 .hold_ms = 40}),
    tone_voice({.wave = Wave::sine,
                .from_hz = 80,
                .to_hz = 48,
                .duration_ms = 200,
                .volume = 0.04f,
                .attack_ms = 8,
                .drive = 0.25f}),
    noise_voice({.duration_ms = 620,
                 .volume = 0.032f,
                 .delay_ms = 30,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 900, .to_hz = 2400, .q = 0.6f},
                 .attack_ms = 40,
                 .hold_ms = 120}),
};

constexpr SoundVoice kHitTreeVoices[] = {
    noise_voice({.duration_ms = 18,
                 .volume = 0.06f,
                 .filter = {.type = FilterType::highpass, .hz = 300},
                 .echo = 0.22f}),
    tone_voice({.wave = Wave::sine,
                .from_hz = 130,
                .to_hz = 110,
                .duration_ms = 220,
                .volume = 0.055f,
                .filter = {.type = FilterType::lowpass, .hz = 800},
                .drive = 0.55f}),
    tone_voice({.wave = Wave::triangle,
                .from_hz = 240,
                .to_hz = 190,
                .duration_ms = 120,
                .volume = 0.02f,
                .delay_ms = 6,
                .filter = {.type = FilterType::bandpass, .hz = 600, .q = 2},
                .drive = 0.4f}),
    noise_voice({.duration_ms = 700,
                 .volume = 0.03f,
                 .delay_ms = 90,
                 .color = Color::pink,
                 .filter = {.type = FilterType::lowpass, .hz = 2400, .to_hz = 700},
                 .attack_ms = 60,
                 .hold_ms = 120}),
};

constexpr SoundVoice kBrushTreeVoices[] = {
    noise_voice({.duration_ms = 220,
                 .volume = 0.03f,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 2600, .to_hz = 1400, .q = 0.8f},
                 .attack_ms = 20}),
    tone_voice({.wave = Wave::triangle,
                .from_hz = 210,
                .to_hz = 170,
                .duration_ms = 70,
                .volume = 0.03f,
                .delay_ms = 30,
                .filter = {.type = FilterType::lowpass, .hz = 900},
                .drive = 0.3f}),
    noise_voice({.duration_ms = 480,
                 .volume = 0.018f,
                 .delay_ms = 110,
                 .color = Color::pink,
                 .filter = {.type = FilterType::lowpass, .hz = 2000, .to_hz = 700},
                 .attack_ms = 60,
                 .hold_ms = 80}),
};

constexpr SoundVoice kBumpVoices[] = {
    noise_voice({.duration_ms = 60,
                 .volume = 0.03f,
                 .filter = {.type = FilterType::bandpass, .hz = 900, .q = 3}}),
    tone_voice({.wave = Wave::triangle,
                .from_hz = 170,
                .to_hz = 95,
                .duration_ms = 200,
                .volume = 0.045f,
                .filter = {.type = FilterType::lowpass, .hz = 1200, .to_hz = 400},
                .drive = 0.4f}),
    noise_voice({.duration_ms = 240,
                 .volume = 0.02f,
                 .delay_ms = 20,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 1400, .to_hz = 2800, .q = 0.9f},
                 .attack_ms = 12}),
};

constexpr SoundVoice kGateVoices[] = {
    noise_voice({.duration_ms = 70,
                 .volume = 0.045f,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 900, .to_hz = 500, .q = 0.8f},
                 .attack_ms = 2}),
    tone_voice({.wave = Wave::sine,
                .from_hz = 170,
                .to_hz = 95,
                .duration_ms = 110,
                .volume = 0.035f,
                .attack_ms = 3,
                .drive = 0.25f}),
    tone_voice({.wave = Wave::triangle,
                .from_hz = 620,
                .to_hz = 560,
                .duration_ms = 240,
                .volume = 0.012f,
                .delay_ms = 40,
                .echo = 0.1f,
                .filter = {.type = FilterType::lowpass, .hz = 2400}}),
};

constexpr SoundVoice kMissedVoices[] = {
    tone_voice({.wave = Wave::square,
                .from_hz = 330,
                .to_hz = 260,
                .duration_ms = 180,
                .volume = 0.03f,
                .filter = {.type = FilterType::lowpass, .hz = 1200},
                .drive = 0.4f}),
    tone_voice({.wave = Wave::square,
                .from_hz = 220,
                .to_hz = 175,
                .duration_ms = 240,
                .volume = 0.028f,
                .delay_ms = 130,
                .filter = {.type = FilterType::lowpass, .hz = 1000},
                .drive = 0.4f}),
};

constexpr SoundVoice kResetVoices[] = {
    noise_voice({.duration_ms = 280,
                 .volume = 0.03f,
                 .color = Color::brown,
                 .filter = {.type = FilterType::lowpass, .hz = 320},
                 .attack_ms = 10}),
    noise_voice({.duration_ms = 240,
                 .volume = 0.018f,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 1400, .to_hz = 2600, .q = 0.8f},
                 .attack_ms = 15}),
};

constexpr SoundVoice kFinishVoices[] = {
    tone_voice({.wave = Wave::saw,
                .from_hz = 330,
                .duration_ms = 700,
                .volume = 0.026f,
                .attack_ms = 20,
                .hold_ms = 320,
                .detune_cents = 8,
                .echo = 0.18f,
                .filter = {.type = FilterType::lowpass, .hz = 2200},
                .drive = 0.5f}),
    tone_voice({.wave = Wave::saw,
                .from_hz = 495,
                .duration_ms = 700,
                .volume = 0.02f,
                .attack_ms = 20,
                .hold_ms = 320,
                .detune_cents = 8,
                .echo = 0.18f,
                .filter = {.type = FilterType::lowpass, .hz = 2600},
                .drive = 0.5f}),
    tone_voice({.wave = Wave::square,
                .from_hz = 1760,
                .to_hz = 1700,
                .duration_ms = 260,
                .volume = 0.014f,
                .delay_ms = 80,
                .echo = 0.14f,
                .filter = {.type = FilterType::bandpass, .hz = 2400, .q = 2.5f}}),
    tone_voice({.wave = Wave::square,
                .from_hz = 1480,
                .to_hz = 1430,
                .duration_ms = 260,
                .volume = 0.014f,
                .delay_ms = 230,
                .echo = 0.14f,
                .filter = {.type = FilterType::bandpass, .hz = 2100, .q = 2.5f}}),
    tone_voice({.wave = Wave::square,
                .from_hz = 1980,
                .to_hz = 1900,
                .duration_ms = 300,
                .volume = 0.012f,
                .delay_ms = 390,
                .echo = 0.14f,
                .filter = {.type = FilterType::bandpass, .hz = 2700, .q = 2.5f}}),
    tone_voice({.wave = Wave::sine,
                .from_hz = 1320,
                .duration_ms = 520,
                .volume = 0.03f,
                .delay_ms = 300,
                .hold_ms = 120,
                .detune_cents = 6,
                .echo = 0.2f}),
};

constexpr SoundVoice kWipeoutTreeVoices[] = {
    tone_voice({.wave = Wave::sine,
                .from_hz = 70,
                .to_hz = 40,
                .duration_ms = 260,
                .volume = 0.065f,
                .delay_ms = 40,
                .attack_ms = 3,
                .drive = 0.6f}),
    noise_voice({.duration_ms = 360,
                 .volume = 0.05f,
                 .delay_ms = 40,
                 .color = Color::brown,
                 .filter = {.type = FilterType::lowpass, .hz = 340, .to_hz = 130},
                 .attack_ms = 6}),
    noise_voice({.duration_ms = 1100,
                 .volume = 0.04f,
                 .delay_ms = 160,
                 .color = Color::pink,
                 .filter = {.type = FilterType::lowpass, .hz = 2600, .to_hz = 600},
                 .attack_ms = 80,
                 .hold_ms = 260}),
    clack(260, 0.028f, 520, 380, 1400),
    clack(390, 0.0196f, 440, 330, 1200),
};

constexpr SoundVoice kWipeoutNoseVoices[] = {
    noise_voice({.duration_ms = 140,
                 .volume = 0.04f,
                 .color = Color::brown,
                 .filter = {.type = FilterType::lowpass, .hz = 500, .to_hz = 250},
                 .attack_ms = 10}),
    tone_voice({.wave = Wave::square,
                .from_hz = 900,
                .to_hz = 700,
                .duration_ms = 18,
                .volume = 0.02f,
                .delay_ms = 70,
                .filter = {.type = FilterType::bandpass, .hz = 1800, .q = 2}}),
    tone_voice({.wave = Wave::sine,
                .from_hz = 74,
                .to_hz = 38,
                .duration_ms = 300,
                .volume = 0.075f,
                .delay_ms = 150,
                .attack_ms = 2,
                .drive = 0.65f}),
    noise_voice({.duration_ms = 360,
                 .volume = 0.052f,
                 .delay_ms = 150,
                 .color = Color::brown,
                 .filter = {.type = FilterType::lowpass, .hz = 320, .to_hz = 120},
                 .attack_ms = 5}),
    noise_voice({.duration_ms = 700,
                 .volume = 0.04f,
                 .delay_ms = 170,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 800, .to_hz = 2600, .q = 0.6f},
                 .attack_ms = 30,
                 .hold_ms = 120}),
    clack(380, 0.03f, 520, 380, 1400),
    clack(510, 0.021f, 440, 330, 1200),
};

constexpr SoundVoice kWipeoutCatchVoices[] = {
    noise_voice({.duration_ms = 110,
                 .volume = 0.045f,
                 .filter = {.type = FilterType::bandpass, .hz = 1600, .to_hz = 3400, .q = 2.5f},
                 .attack_ms = 8}),
    tone_voice({.wave = Wave::sine,
                .from_hz = 76,
                .to_hz = 40,
                .duration_ms = 280,
                .volume = 0.07f,
                .delay_ms = 220,
                .attack_ms = 2,
                .drive = 0.6f}),
    noise_voice({.duration_ms = 320,
                 .volume = 0.048f,
                 .delay_ms = 220,
                 .color = Color::brown,
                 .filter = {.type = FilterType::lowpass, .hz = 340, .to_hz = 130},
                 .attack_ms = 5}),
    noise_voice({.duration_ms = 800,
                 .volume = 0.03f,
                 .delay_ms = 280,
                 .color = Color::pink,
                 .filter = {.type = FilterType::bandpass, .hz = 1600, .to_hz = 900, .q = 0.7f},
                 .attack_ms = 60,
                 .hold_ms = 200}),
    clack(420, 0.026f, 520, 380, 1400),
    clack(550, 0.0182f, 440, 330, 1200),
};

constexpr SoundDef kDefs[] = {
    {kLandSoft,
     "Back on the snow: the legs taking it, a short low sine thump bending down under a brown "
     "settle and a pink puff thrown up by the skis, all opening over a few milliseconds, because "
     "snow has no transient.",
     kLandSoftVoices},
    {kLandHard,
     "A landing the legs could not take: the knees folded to their stops, a hard driven thump "
     "under the one white knock a landing is allowed, then a bigger and longer sheet of snow.",
     kLandHardVoices},
    {kLandPowder,
     "Into deep loose snow: the whumpf. No knock at all, a low brown swell opening as the skis go "
     "under, a sine under it for the legs, then a long pink cloud of powder hanging in the air.",
     kLandPowderVoices},
    {kHitTree,
     "Into a trunk: the crack of wood, broadband and gone in a fiftieth of a second, over the "
     "body's hollow driven thump; then the tree answers, its load shaken down a beat later as a "
     "pink sheet. The woods echo the crack, not the fall.",
     kHitTreeVoices},
    {kBrushTree,
     "A trunk brushed, not met: the shoulder through the lowest boughs, a soft pink swish, a light "
     "knock on the bark with no crack in it, and a little snow sifting down after. The warning, "
     "not the wreck.",
     kBrushTreeVoices},
    {kBump,
     "Skier on skier: two bodies shouldering and the skis clacking, a short driven triangle "
     "bending down under a band of white for the knock and a thin pink spit of snow. Rounder than "
     "a trunk, because nothing here is hard.",
     kBumpVoices},
    {kGateTaken,
     "A gate taken: the panel slapped past on its hinged pole, a short pink whack with a hollow "
     "sine body under it, and the pole's spring ringing back for a moment with a touch of the "
     "hill's echo.",
     kGateVoices},
    {kGateMissed,
     "A gate skied past: two driven squares falling, the second lower and later, dry and dark. "
     "Bad news does not ring.",
     kMissedVoices},
    {kReset,
     "Stood back on the piste: a soft settle into the snow, a brown thump and a short pink sigh, "
     "and nothing else, because the wind is already there when the picture lands.",
     kResetVoices},
    {kFinish,
     "The finish line: the arena's horn, a driven sawtooth and its fifth held on the echo, three "
     "cowbell clanks under it a little apart, and a held chime on top so it still reads as the "
     "last gate.",
     kFinishVoices},
    {kWipeoutTree,
     "Thrown at a trunk's foot (the crack is the hit beside it): the body crumpling into the snow "
     "round the roots, a heavy driven thud with brown mass, the tree's load coming down on him a "
     "beat later, and the skis clattering off.",
     kWipeoutTreeVoices},
    {kWipeoutNose,
     "Over the tips on landing: the ski fronts dig in and shove a brown wedge of snow, the "
     "bindings let go with a click, the body goes face first into the slope as the biggest thud "
     "in the bank under a pink burst, and the skis clatter off.",
     kWipeoutNoseVoices},
    {kWipeoutCatch,
     "An edge caught, thrown across the skis: the steel ripping sideways for a fraction of a "
     "second, a rising white band, then the body slammed down on the far side a beat later, a "
     "sliding pink hiss, and the skis clattering off.",
     kWipeoutCatchVoices},
    {kMenuFocus,
     "The menu's focus moved: a short wooden tick, dry and small under the wind, so a held arrow "
     "reads as a ratchet and never as an alarm.",
     kMenuFocusVoices},
    {kMenuPress,
     "An item pressed: a low knock with a bright tick on it, the press of a boot clicking into "
     "its binding.",
     kMenuPressVoices},
    {kMenuPage,
     "A page opened or closed: a short pink breath of snow swept aside, rising, with no edge.",
     kMenuPageVoices},
};

}  // namespace

const std::span<const SoundDef> kBank = kDefs;

}  // namespace fall_line::sound

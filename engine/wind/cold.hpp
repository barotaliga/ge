// WHAT THE AIR'S TEMPERATURE DOES TO THINGS (docs/climate.md § Effects): the marks the cold (and
// the thaw) leaves, as pure functions of the air (climate.hpp) and the surface under it, for the
// world, its snow, its water and its picture to read.
//
// - DEW, FROST AND RIME (`deposit_on`): a surface under the air's dew point gathers dew; under the
//   frost point and below 0 C it grows hoar frost (feathers of ice straight from the vapour); in
//   freezing fog with a wind the supercooled droplets freeze on its windward side as rime. The
//   rate is the deficit times a coefficient (about 0.03 mm an hour a kelvin), what a clear night
//   lays on a car's roof. It goes again (`frost_loss`) as the surface warms: sublimed at the same
//   rate a kelvin over the frost point, melted at 0.3 mm an hour a kelvin over 0 C.
// - BLACK ICE (`glaze_on`): rain on a surface below 0 C freezes into a clear glaze.
// - BREATH (`breath_of`): exhaled air (34 C, saturated) mixing into the air; where some mix of
//   the two is supersaturated it condenses. What shows is the most liquid water a mix holds past
//   the faint cloud a 10 C, near-saturated air's mix makes, as a share of what a winter's -10 C
//   breath holds: 0 unseen (above about 10 C in damp air, 5 C in dry), 1 a -10 C plume, more
//   when colder.
// - ICE ON WATER (`ice_grown`, `ice_bears`): Stefan's law over the freezing degree-days, h = a
//   sqrt(FDD) with a = 0.024 m per root kelvin-day (a lake under a little snow); a thaw melts it
//   at 6 mm a kelvin-day. What it bears is Gold's formula, P = A h^2 with A = 1.75 kg/cm2 (the
//   cautious end of clear ice: 10 cm bears a walker, 25 cm a car).
// - THE SNOW'S FEEL (`snow_feel`, `fresh_density`, `glide_of`): new snow falls lighter the colder
//   the air (Hedstrom and Pomeroy: 67.9 + 51.3 e^(T / 2.59) kg/m3 below 0 C, 119 + 20 sqrt(T)
//   above); a ski glides worst in very cold snow (dry friction off sharp crystals), best a few
//   degrees under freezing (a melt film the ski makes itself), and worse again in wet snow (the
//   water's suction); snow squeaks underfoot below about -10 C. The feel's names are a world's
//   words for the sound, the picture and the player: dry powder, cold and squeaky, soft, sticky
//   (balls and packs), wet slush, and crust (refrozen after a thaw). engine/snow's layer is
//   handed the air (`Weather::air`) and `fresh_density`; its melt and crust follow from them.
//
// ge:summary The cold's marks: dew, frost, rime, black ice, breath, lake ice, the snow's feel
// ge:keywords frost hoar-frost dew rime glaze black-ice breath condensation lake-ice ice
// ge:keywords stefan freezing-degree-days gold bearing snow-feel powder slush sticky squeak
// ge:keywords crust glide ski-friction fresh-snow-density hedstrom deposit_on glaze_on
// ge:keywords frost_loss sublimation breath_of ice_grown ice_bears snow_feel fresh_density glide_of
// feel_name ge:skill water determinism ge:test tests/wind_cold_test.cpp
#pragma once

#include <cstdint>

#include "wind/climate.hpp"
#include "wind/mist.hpp"

namespace ge::wind {

enum class Deposit : uint8_t { none, dew, frost, rime };
// "none", "dew", "frost", "rime".
const char* deposit_name(Deposit d);

struct Settling {
  Deposit kind = Deposit::none;
  double rate = 0;  // mm/h of water (dew) or ice (frost, rime) on the surface
};

// What the air lays on a surface at `surface_c` (surface_at, or the game's own: a car's roof, a
// railing): dew, hoar frost, or rime in a freezing fog with a wind of `wind` m/s.
Settling deposit_on(const Thermal& air, double surface_c, const Mist& mist, double wind);
// How fast frost (or rime) on a surface at `surface_c` goes, mm/h of ice: sublimed into air whose
// frost point is under the surface, melted once the surface is over 0 C; 0 while it still grows.
double frost_loss(const Thermal& air, double surface_c);

// The clear ice rain lays on a surface at `surface_c` under a fall whose rain share is `rain`,
// 0..1: 0 no glaze .. 1 a full glaze (the surface well under freezing in plain rain).
double glaze_on(double surface_c, double rain);

// How thick the breath's cloud shows: 0 unseen .. 1 a -10 C winter's plume (more when colder).
double breath_of(const Thermal& air);

// Ice `h0` m thick after `days` at a daily mean of `mean_c`: grown by Stefan's law below 0 C,
// melted above. Water itself starts freezing only once it is at 0 C (`water_c`).
double ice_grown(double h0, double mean_c, double days);
// What ice `h` m thick bears, kg.
double ice_bears(double h);

enum class Feel : uint8_t { powder, squeaky, soft, sticky, slush, crust };
// "powder", "squeaky", "soft", "sticky", "slush", "crust".
const char* feel_name(Feel f);

// The snow's feel at a snow surface of `snow_c` (surface_at's, never above 0) under air at
// `air_c`: `fresh` m of new snow lies on it, and it `thawed` since it last froze (a crust when the
// cold has come back).
Feel snow_feel(double snow_c, double air_c, double fresh, bool thawed);

// New snow's density, kg/m3, falling through air at `air_c`.
double fresh_density(double air_c);
// A ski's kinetic friction on snow at `snow_c` with a liquid share `wet` (0 dry .. 1 slush).
double glide_of(double snow_c, double wet);
// Whether snow at `snow_c` squeaks underfoot.
inline bool squeaks(double snow_c) { return snow_c < -10; }

}  // namespace ge::wind

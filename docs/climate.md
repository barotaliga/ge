# Temperature and humidity

How warm and how damp the air is, and what that does to a world: the fog, whether a fall is snow
or rain, how the snow feels, dew, frost, rime, black ice, the breath and the ice on a lake. Roadmap
rows M51–M51e ([`goals.md`](goals.md)); the code is `engine/wind/climate.hpp`, `mist.hpp` and
`cold.hpp` (in `ge_wind`, skill `water`); the lab is `climate`.

Everything here is a **pure function** of data a world sets once (a `Climate`), the place it asks
about (a `Site`) and the solar hour. Nothing is stepped, nothing draws a random number, and only
`core::libm` is called, so every machine, replay and lockstep peer reads the same air, and a world
that never asks pays nothing.

## The model

A `Climate` is a day's air at a place:

| Field | Unit | What |
| --- | --- | --- |
| `mean_c` | C | the midpoint of the day's low and high, 2 m over open ground at `ref_y` |
| `range_c` | K | the low-to-high swing under a clear, calm sky (10 to 15 in a dry inland autumn) |
| `ref_y` | m | the height `mean_c` is given at |
| `lapse` | K/m | how fast the air cools upward (0.0065, the standard atmosphere) |
| `humidity` | 0..1 | the relative humidity at the day's mean: it fixes the day's dew point |
| `cloud` | 0..1 | the sky's cover |
| `wind` | m/s | the mean 10 m over open ground (`wind::Field::mean`) |
| `sunrise`, `sunset` | h | solar hours (the day's length is the season) |
| `water_c` | C | open water's surface, which lags the air by weeks |

A `Site` is where the question is asked: its height `y`, its height `above` the ground or water
under it, how far it lies in a `hollow` below the ground round it, the distance to open water
`water_m`, the ground's standing water `wet` and snow cover `snow`, the share of the `sky` it sees
(0 indoors) and any local `warmth` (a fire, a lava pool).

`air_at` answers with a `Thermal`: the temperature, the dew point, the frost point, the relative
humidity and the wet-bulb temperature.

- **The day's curve.** The low comes at sunrise. Through the day the temperature rises as a sine
  that peaks two hours past solar noon; through the night it falls exponentially (b = 2.2 over the
  night's length) back to the next sunrise's low. The swing is `range_c` damped by cloud (to 0.3
  overcast), by wind (to half at 8 m/s) and by a roof (to 0.15 indoors).
- **Height.** The air cools at `lapse` upward; the dew point falls at 1.8 K/km (`kDewLapse`), so
  rising air meets its dew point about 125 m up for every kelvin of spread: a mountain in its
  cloud.
- **The night's cold pool.** On a clear, calm night cold air drains off the slopes into low ground:
  0.02 K a metre of `hollow`, up to 12 K, built over the three hours after sunset and broken within
  two after sunrise (`night_chill`: clear squared, times 1 / (1 + (wind / 2.5)^2)). A valley's
  floor is the coldest place on a still night, colder than the summit.
- **The dew point.** The day's moisture is a dew point, held while the temperature swings, so the
  relative humidity climbs at night and falls by day. Open water (within about 150 m) and wet
  ground evaporate into the air, raising the dew point toward the wet surface's own temperature
  (never past the air's). A lake also tempers the air toward its own temperature (30 % of the gap
  at the shore, fading over 300 m). What a surface lends the air fades with the height over it
  (1/e in 50 m): the surface layer.
- **Vapour pressure.** The Magnus formula over water (6.1094 exp(17.625 T / (T + 243.04)) hPa)
  and over ice (6.1121 exp(22.587 T / (T + 273.86))) gives the dew point, the frost point and the
  humidity. The wet bulb is solved from the psychrometric balance at the height's pressure.
- **The surface** (`surface_at`): colder than the air on a clear night (up to 4 K, radiating to
  the sky; less in wind; the cold fading over the two hours after sunrise, never a jump), warmer by day in the sun on bare ground (up to 5 K); snow never above 0;
  open water at `water_c`.

## Snow or rain

`rain_share_of` reads the **wet bulb**, not the air: a fall cools the air it falls through by
evaporating, so in dry air snow reaches the ground at +3 C and more, and in saturated air it turns
to rain round +1 C. The share is 0 under a -0.5 C wet bulb and 1 over +2.5 C, a smooth S between
(half and half at +1 C). `freezing_level` is where the open air crosses 0 C at an hour (what
`wind::rain_share_at` reads as its freezing level); `snow_level` is where the wet bulb is +1 C,
lower than the freezing level by a few hundred metres in moist air and by more in dry air.

## Fog

`mist_at` answers with a `Mist`: its kind, its strength, its density (the extinction at its floor,
1/m; the visual range is 3 / it, as `render::FogVolume` reads it), its depth, its visual range,
how patchy it is (`noise`), how fast its wisps rise and whether it is freezing. The strongest of
five kinds wins:

| Kind | When | Look |
| --- | --- | --- |
| radiation | a clear, still night cools the air past its dew point; strongest where water, wet ground or a hollow keeps feeding it; a breeze of 1 to 2 m/s deepens it, past 5 m/s the wind mixes it away; the morning sun burns it off | 5 to 45 m deep, an even bank |
| valley | the same in a hollow (over 20 m deep) | a quarter as deep as the hollow, to 150 m more, laid LEVEL like water: the slopes above it stand clear |
| hill | air lifted into its own cloud: by height, or saturated under an overcast | 200 m, the mountain in cloud |
| steam | air 6 K and more colder than open water over it: sea smoke, indoors as outdoors, in wind or calm | 2 to 15 m of rising wisps |
| advection | moist air blown over a colder surface: a cold lake, a snowfield in a thaw; needs a wind to bring it, lifts into low cloud in a gale | 30 m and more |

On dry ground most of the water a cooling night squeezes out settles as dew or frost and the
drainage off the slopes mixes the rest away, so a dry slope keeps at most a thin mist. The fog's
water is the air's own: the same saturation lays a thicker fog with a 5 C dew point than with a
-15 C one (the vapour pressure is four times higher). Fog below 0 C is freezing fog: supercooled,
it rimes what it touches. A fog still forming (strength under `kFormed`, 0.3: a breeze's last
wisps, a morning's thinning bank) is not yet a bank: its visual range eases in the logarithm from
the clear air's 20 km to its kind's as it strengthens, so a windy dawn by a lake veils a view
hardly at all while a still one lays a bank 100 to 200 m deep in the eye (M51c).

**So fog is thickest** on a clear, still night after a damp day, by water or over wet ground or
melting snow, in low ground, from about 0 C up; it lifts when the wind rises, the cloud comes over
or the morning sun warms the air past its dew point. In the bitter cold an open lake steams
whatever the humidity.

## What the cold does (`cold.hpp`)

- **Dew, frost and rime** (`deposit_on`): a surface under the dew point gathers dew; under the
  frost point and below 0 C hoar frost; in a freezing fog with a wind, rime on its windward side.
  About 0.03 mm an hour a kelvin of deficit; rime 0.2 mm an hour in a thick fog at 5 m/s.
- **Black ice** (`glaze_on`): rain on a surface below 0 C, full at -2 C.
- **The breath** (`breath_of`): exhaled air (34 C, saturated) mixing into the air condenses where
  a mix is supersaturated. 0 unseen (above about 10 C in damp air, 5 C in dry), 1 a -10 C winter's
  plume, more when colder.
- **Lake ice** (`ice_grown`): Stefan's law, h = 0.024 sqrt(FDD) m over the freezing degree-days; a
  thaw melts 6 mm a kelvin-day. `ice_bears` is Gold's formula with the cautious A = 1.75 kg/cm2:
  10 cm bears a walker, 25 cm a car.
- **The snow** (`fresh_density`, `glide_of`, `snow_feel`, `squeaks`): new snow falls at
  67.9 + 51.3 e^(T / 2.59) kg/m3 below 0 C (68 in the cold, 119 at 0 C) and 119 + 20 sqrt(T) above,
  to 200. A ski's friction is lowest a few degrees under freezing (0.025, a melt film the ski makes
  itself), climbs into the cold (dry friction off sharp crystals: 0.08 at -20 C) and with the snow's
  water (suction). The feel's words: powder (new snow under -6 C), sticky (new snow over -2 C, or
  old snow at 0 C), soft, squeaky (under -10 C), slush (melting) and crust (refrozen after a thaw).

## Who reads it

Each reader is a row of its own; the engine gives the air and the effects, a world decides where
it asks.

| Reader | Reads | Row |
| --- | --- | --- |
| The fog's areas (`render/fog.hpp`, M38) | `mist_at` at a world's sites, handed over by `render/mist_fog.hpp` (`mist_volume`: a bank as deep as `depth`, steam thinning upward in wisps climbing at `rise`; `mist_valley`: a valley's fog level from its floor); Fall Line's valleys, ground and summit and any lake laid on its map (`water_mist`), a classic map's pools of water and slime (`bsp_fog.hpp`'s `FogRows::air`) | M51c (done) |
| The snow layer (`engine/snow`, M29) | `Weather::air` sampled off `air_at`; `fresh_density` for `Weather::fresh_density`; `glide_of` and `snow_feel` for the contact and the sound (`engine/snow/air.hpp`: `weather_of`, `feel_at`) | M51d (done) |
| What falls (`precip.hpp`, M36) | `freezing_level` or `snow_level` as the freezing level; `rain_share_of` at the ground | M51e |
| The water (`engine/water`, M24) | `ice_grown`, `ice_bears`, steam fog over open water | M51e |
| The classic levels (`plugins/classicenhance`, M41) | a `Climate` per level | M51b |

## Considered and not taken

- **A stepped air** (temperatures integrated through a day as state): it would have to be saved,
  replayed and sent to every peer, and the day's curve with a cold pool reproduces what a world
  needs. Where a history matters (the snow's crust, the lake's ice), its own module integrates a
  schedule of `air_at` or a mean.
- **A full boundary-layer model** (turbulence closures, soil heat): orders of magnitude more cost
  for differences no player sees.
- **Fog straight from relative humidity**: every clear night in damp weather fogs everywhere; the
  supply of water (a lake, wet ground, a hollow) is what decides where the bank lies.

## Labs

`make lab LAB=climate` (flags `--mean`, `--humidity`): the valley's night in six panels, the day's
air, the fog through the night, where fog lies by temperature against humidity and against wind,
the phase, the snow's density and glide, the breath and lake ice (`docs/labs.md`).

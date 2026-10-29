# Water and wind

The plan for water in the engine: bodies of water a game lays down in one call, the waves the
wind grows on them, things that float, people who swim, water that flows (rivers, waterfalls,
a basin emptying through a broken wall), and how all of it is drawn above and below the
surface and heard. Wind arrives with it as an engine-wide concept, because the waves are the
wind's and so are a flag, a tree's sway, a paraglider's day and a particle's drift.

The roadmap rows are M24a–M24l in [`goals.md`](goals.md); each row's status is
`goals/M24<x>.md`. This page is the design those rows build to. Every number in it is a
starting value: the row that builds it measures it in its lab and corrects it here.

## What the games ask of it

| Game | Water it needs | Swimming | Flow |
| --- | --- | --- | --- |
| Fall Line (skiing) | Mountain lakes and ponds (open, freezing, frozen, thawing on the map's day), streams and rivers off the real faces' maps, the valley's reservoir; skimming a pond on skis or a snowmobile at speed; the view of a lake mirroring the mountain | A skier who falls in sinks slowly: clothes soaking, ski boots filling, cold water taking his strength | Streams down the mountain; meltwater; a waterfall off a cliff band |
| Shooter's Edge (first person) | Water, slime and lava volumes in a brush world; warped surfaces in the classic look and real water in the modern one | The classic first-person swim feel, to the unit: up and down by the view's pitch, slow sinking when idle, friction by how deep, the jump out at a ledge | A modern-look extra: a wall broken and a flooded room draining |
| A watercraft racer (later) | The open sea and its storm, the coast and its surf, river mouths, the wakes the craft leave and ride | A rider thrown off his craft | Rivers with current; breaking waves on a shore |
| Any game | `add_lake`, `add_ocean`, `add_river`, `add_volume`: one call each, a look chosen by name | The engine's default swimmer | A flow region round anything that should spill |

## Homes (G8)

Water and wind are generic: any genre that has weather or a lake uses them unchanged, so they
are engine modules, not a plugin. Only the tuned numbers that make one game's swim or one
world's water are in that game or plugin.

| Home | Holds |
| --- | --- |
| `engine/wind/` | The wind as a pure function of place and time: the mean, the gusts and the veer, the height profile, a sheltered plan, transient local sources (M24a) |
| `engine/water/` | No GPU, simulation code: the bodies and the one query, the spectrum and the waves, the fetch, floating, swimming, the flowing grid, the optics (the colour as numbers) |
| `engine/render/water*` | The water drawn: the surface's geometry tiers, its material, the GPU wave detail, the ripples, the foam, the caustics, the view under water, the waterline, the moving water's curtains and flow maps |
| `engine/audio/` | The water's voices: lapping, a river, a waterfall, a splash, the muffle under water |
| `plugins/shooter/` (M18e) | The classic swim preset and the water levels a brush world's contents give |
| `plugins/skier/` | The clothed skier's swim preset: the suit and boots soaking, the skis' own float |
| `plugins/mountain/` | Where a mountain's water is (a real face's lakes and streams, a tarn in a cirque) and the mountain's wind as a source of the engine's |
| `games/fall_line/` | Which water a map gets, lake ice on the map's day, falling in, skimming, the bot near water, the look's tuning |

## Wind (`engine/wind`, M24a)

Today the only wind is the mountain's (`plugins/mountain/air`): a mean dealt by the map, three
gust sines and a veer, folded so it never blows up the slope. M24a makes wind an engine
concept with the mountain's as one configuration of it, and every digest holds: the mountain's
`wind_at` keeps its bits, read through the engine's function from a spec with its own
periods, shares and hashed phases, and the downhill fold stays in the mountain, which is the
only place that knows what a slope is.

**A wind is a pure function** `wind_at(field, p, t)` of a `WindField` (data, built once) and
a point and a time. Nothing is stepped, so a replay, a ghost, a lockstep peer and a server
all read the same air with no state to send.

- **The mean**: a speed at 10 m over open ground and the bearing it blows from.
- **Gusts and veer**: a short list of components (a period, a share of the mean, a phase
  hashed off the field's seed), summed. The default set is spread over the turbulence
  spectrum's energetic band (6–60 s); a game may lay its own.
- **The height profile**: the log law, U(z) = U₁₀ · ln(z / z₀) / ln(10 / z₀), with the
  ground's roughness length z₀ (about 0.0002 m over open water by Charnock's relation at
  moderate winds, 0.03 m over snow and grass, 0.5–1 m over forest and towns). Below z₀ the
  wind is zero.
- **Shelter**: an optional baked grid over the plan, each cell a multiplier and a turn
  (a forest, a town, a ridge's lee), read bilinearly. A cell of tens of metres: the air does
  not step at a wood's edge.
- **Local sources**: a small fixed array of transient sources the game passes in from its own
  state (a rotor's downwash, a blast, a vehicle's slipstream, a whirl), each a closed-form
  velocity field with a radius. They come from state, so they are as deterministic as the
  state is.

Readers: the waves (the mean only: a sea is grown by hours of wind, not this second's gust),
the particles and the falling snow, the clouds' drift, a body's aerodynamic drag, the trees'
sway, the sound of wind, and every machine that flies.

## Bodies of water and the one query (`engine/water`, M24b)

A `water::World` holds the water a game has laid down. **Every question anything asks of water
goes through one query**, so the physics, the swimmer, the sound and the renderer can never
disagree about where the surface is:

```cpp
water::Sample s = water::sample(world, p, t);
// s.body      which body (or none)
// s.surface   the surface's height over p at t, m (waves, flow and all)
// s.normal    the surface's normal there
// s.velocity  the water's own velocity at p: the waves' orbital motion, the current, the flow
// s.depth     how far p is under the surface (negative above it)
// s.bed       the bed's height under p (depth of the water column = surface - bed)
// s.density   kg/m³ (1000 fresh, 1025 sea); s.temperature, °C; s.optics, the look's row
```

Batch forms take spans of points so a hull's probes or a crowd's feet cost one call. The kinds:

| Kind | Laid by | Surface | Typical use |
| --- | --- | --- | --- |
| Lake | `add_lake(world, outline, level, look)` | A level plane plus its waves | A pond, a tarn, a reservoir, a pool |
| Ocean | `add_ocean(world, sea_level, look)` | An unbounded plane plus a full sea | A coast, an island, a horizon |
| Channel | `add_river(world, centreline, widths, discharge, look)` | A surface falling down a polyline, a current across it (fastest mid-stream, slowed at the banks) | A stream, a river, a canal: cheap and steady |
| Volume | `add_volume(world, hulls, look)` | The top faces of convex hulls (a brush world's water, slime and lava) | Shooter's Edge's contents; any boxed water |
| Flow region | `add_flow(world, box, cell, bed, look)` | The flowing grid's (M24f) | Water that spills, floods, empties and falls |

A body carries its look by name (`"nordic-brown"`, `"adriatic"`, … — § Colour) and its
salinity and temperature. A lake or a volume can be **woken into a flow region** when the game
breaks something that held it (§ Flowing water): the plane becomes a depth over the bed and
the grid takes over.

## Waves (`engine/water`, M24b)

**One spectrum, three tiers, and the waves you feel are the waves you see.**

- **The spectrum.** The wind's 10 m mean over the body's fetch grows a sea: the
  fetch-limited JONSWAP spectrum (a peak enhancement γ ≈ 3.3) with its significant height and
  peak period from the fetch laws, capped at the fully developed Pierson–Moskowitz sea; a depth
  correction (the TMA form) for shallow water; a directional spread about the wind (cos²ˢ,
  narrow at the peak and widening above it); an optional swell from elsewhere. **The fetch** is
  measured per point over the body's outline on a fan of rays upwind and averaged by cos weights
  (the effective fetch), so a long lake rough at its lee shore and calm at its windward one, and
  a pond grows ripples, not swell.
- **The simulation tier** (CPU, deterministic, `core/libm` only): the spectrum's energetic
  part laid as 16–32 components, cut into slices of equal weight, the energy times
  1 + (ω/ωp)² (they crowd onto the peak, so the peak is a wave train, not one sine beating
  against an octave-off neighbour, and the tail keeps enough short waves that no single one
  stripes the surface; each carries its own slice's energy, so Hs is exact) and their
  directions drawn through the spread by stratified inverse transform. Each component is a
  linear wave with its second-order (Stokes) correction, η = a·sin φ − ½·k·a²·cos 2φ, which
  gives the sharp crest and flat trough and keeps the height a function of the undisplaced
  point, so a probe never has to invert a horizontal displacement. Dispersion with depth,
  ω² = g·k·tanh(k·d), shortens and slows waves over a shelving bed, and shoaling grows them.
  This tier is what every probe, swimmer and floating body reads.
- **The detail tier** (GPU, visual only): an FFT ocean over two or three cascades that holds
  only the spectrum ABOVE the simulation tier's band, with its horizontal (choppy)
  displacement and the Jacobian of it for the foam. The GPU also evaluates the simulation tier's
  components (in 32-bit, the same formula), so a crest that lifts a boat is the crest drawn
  under it, and what the GPU adds alone is too small to lift anything.
- **The interaction tier** (GPU, visual only): a small wave-equation height field round the
  camera (about 64 m) into which bodies, swimmers, rain and splashes drop ripples; it fades at its
  rim.

Breaking: a crest steeper than about 1/7 spills; a wave whose height passes about 0.78 of the
depth breaks on the shoal; over a fresh breeze (about 8 m/s) the wind blows the tops off. Each
is a 0..1 share the foam reads (§ Drawn).

**Wakes** (M24i): a hull moving through water as a line of point sources, each a ring packet
riding out at the group speed with its crests at the phase speed, thinning as 1/√d; they add to
the simulation tier, so a rider is lifted by another's wake, and the V opens at the Kelvin
angle (about 19.5°) by itself.

**Built (M24i, `engine/water/wake.hpp`).** A ring is the deep-water answer to an impulse at a
point (the Cauchy–Poisson ring): at a distance r and a time τ after it, its local wave has
k = gτ²/4r², so its waves run out at their group speed, the long ones first, and its height is
the volume pushed aside times k/(√2 π r), times its source's spectrum exp(−k²b²/4) for a source
b m across (a body makes few waves much shorter than itself). The formula is a far field, so a
ring's first half turn of phase behind its front is faded in. `wake_trail` sets a ring off under
a moving hull each time it has run a third of the shortest wave kept, which is an eighth of the
transverse wave 2πU²/g: the DIVERGING waves run from two thirds of the transverse wave at the arms
to ever shorter toward the track, so the floor decides how much of them is drawn, and rings a third
of a wave apart are what keeps the sum from aliasing into ridges along the track (a slow hull sets
off rings closer together than a fast one). Each ring carries the beam times the draught times
the distance run, and its source is a tenth of the hull's length; `splash_ring` sets off one for
a splash. Summed, the rings make Kelvin's pattern: transverse waves curving across the track and
short diverging crests feathered outward along both arms (the `float` lab draws Kelvin's crest
lines over the plan). The arms' outer edge, where the waves fall to half their highest, is at
19.9° for a 4 m hull at 3 m/s (held within 2° by `tests/water_wake_test.cpp`), narrowing at higher
Froude numbers (15.2° at 6 m/s), as wakes measured on real hulls do. A first version kept no wave
shorter than half the hull: only the transverse waves survived, and the wake read as arcs across
the track with nothing along its arms. The rings live in `World::wakes`, each on its body, faded
over the last third of their life; `sample` adds their height, slope and orbital velocity to a
lake's or a sea's, so a float rides on them.

**A body through the surface (M24h2, `engine/water/entry.hpp`).** A body that falls in is answered
by its own size: `entry_of` takes its mass, volume (so its density), its footprint in the surface
and its velocity, and returns one ring, a crater on it, and the spray's numbers; nothing is
stepped on a grid and nothing is kept but the ring, so it costs the same as a splash and only what
is seen of it is evaluated. From the kinetic energy E = ½mv² down through the surface:

- **The crater.** A hemisphere of water of radius R lifted to the surface stores πρgR⁴/4; a share
  (`kCraterShare`, 0.35) of E goes into it, so R = (4·0.35·E/πρg)^¼, never narrower than the
  body's footprint. Its depth is 0.6R times the square root of the body's density over the
  water's, held between a half and one and a half (a dense body carries on down, a light one bobs
  up). Its shape is (1 − ρ²)e^(−ρ²) with ρ = r/R, a cavity with a raised rim that moves no water;
  it opens and closes over 2√(R/g) s, then the closing throws a jet up the middle, 0.6 of the depth
  high and a third of the crater across, up and down ballistically. It is the ring's near field
  (`crater_height`), added by `ring_height` and `wake_at` and drawn by the water pass in the same
  formula, so the drawn cavity is the one a float drops into.
- **The rings.** The waves are the far field of the cavity's volume (⅔πR² times its depth), set
  off over a source as wide as the crater, so a heavier or faster body throws longer, higher
  waves and a pebble short ripples.
- **The spray.** A crown of drops round the rim at up to 0.8 of the entry speed (two drops a
  joule's square root, 8 to 160) with a little mist, and the jet's column of drops when the crater
  closes (`render/water_stream.hpp`'s `entry_spray` and `jet_spray`). A body coming out sets off a
  ring of its volume's share and a curtain of drops, no crater.

Dropped from 2 m (6.3 m/s) into still water, the `gpu-plunge` lab's table: a 50 g pebble opens
0.08 m and sends 0.1 cm waves 2 m out; a 20 kg crate (it floats) 0.37 m, 1 cm; the 90 kg player
0.53 m, 3.3 cm; a 400 kg boulder 0.77 m, 7.6 cm. The player jumping in off a bank in the classic
move enters at 10.8 m/s: a crater 1.4 m across and 0.42 m deep, 145 drops, waves of 4.7 cm at
2 m. A grid simulation of the surface (a height field stepped by the wave equation, or a
convolution of the ripples) was not taken: it costs a grid round every splash every frame, has to
be absorbed at its edges, and the rings already carry the waves out at the right speed for any
length (§ Not taken).

## Floating (`engine/water/float`, M24c)

A floating body is a set of **probes**: spheres (or boxes) with a volume each, fixed in the
body's frame. For each probe at a step: the submerged share from `sample`'s depth, the buoyancy
ρ·g·V_sub at its centre, a drag against the water's own velocity (linear at low speed,
quadratic above, by the probe's area), an entry slam on the step it first wets, and an added
mass share. The function returns the force and the torque about the body's centre; the caller
integrates its own body, so a sled, a hull, a log, a crate and a soldier all float through the
same code. **Planing** probes carry a flat plate's lift by speed and trim (Savitsky's method in
its simplest form), so a fast snowmobile or a skier at speed skims a pond and slows into it.
Every probe that wets or breaks the surface reports a **splash event** (where, how fast, how
big), which the renderer's particles and ripples and the sound read.

The checks: a box of density 500 kg/m³ floats half under; a body's heave period matches
2π·√(m / (ρ·g·A_waterplane)) within a few per cent; energy decays and never grows.

## Swimming (`engine/water/swim`, M24d)

**One swimmer model, two regimes, tuned per game by one struct.** A game picks a preset or
writes its own; everything a swimmer is lives in `SwimSpec`, with units.

The swimmer is sampled at three heights (feet, waist, eyes) into an **immersion level**
0–3 (dry, wading, swimming, under). The level drives friction, control and breath.

- **The kinematic regime** (the classic first-person feel): velocity-driven, frame-rate
  independent on the fixed step. The wish direction includes the view's pitch (and up and
  down keys); the wish speed is scaled (the classic 0.7 of the run speed); an
  acceleration toward it and a friction scaled by the immersion level; when there is no input
  the swimmer **sinks at a slow fixed speed**; at the surface a jump out onto a ledge in front
  ("the water jump") lifts him over the lip. No buoyancy, no drag curves: the numbers are the
  feel. `plugins/shooter` holds the classic preset (M18e) and its lab holds its trajectories to
  the recorded ones.
- **The body regime** (physical): force-driven through the same probes as floating. The body's
  density against the water's (a person with air in the lungs floats just barely), what he is
  wearing, the cold, his stamina:
  - **Clothes and gear**: each garment has a dry mass, a water capacity and a soak rate, and
    traps air that escapes over the first minutes. A dressed skier floats a little at first,
    his jacket holding air, then the suit soaks, the boots fill (a ski boot holds about a litre
    and a half) and his net buoyancy goes negative: **he sinks slowly** unless he swims, and
    swimming is weak in a soaked suit (drag area up, stroke thrust down). The skis are the
    game's to take off and float (a float of their own, M24c).
  - **The cold**: the water's temperature drives cold shock (the first minute: the gasp, no
    breath held, stroke control poor), then swim failure as the limbs cool (in water near 0 °C
    within about ten minutes of real swimming), then exhaustion; the game decides what each
    stage costs (damage, death, a rescue).
  - **Breath**: the air held and how it runs out by effort and cold shock; the game decides
    what drowning does.
  - **Climbing out**: a ledge test against the game's own collision through a callback, since
    the engine does not know the world's solids.

Presets: `water::default_swimmer()` (a person in a swimsuit in temperate water), the classic one
in `plugins/shooter`, the clothed skier in `plugins/skier`. The swimmer emits the same splash
events as a float, plus strokes and breaths for the sound.

The body regime treats the swimmer as an upright column of his own height (eyes plus 12 cm),
submerged by the share under the surface, with the lungs full while a breath is held, at their
residual once it is gone and between at the surface. What it gives, measured by the `swim` and
`skier-swim` labs: a swimmer in a swimsuit treads water with his eyes a few centimetres out;
lying still, his breath runs out in about a minute (about 20 s in 2 °C water, the cold shock
draining it three times faster) and he sinks at about 0.65 m/s. A dressed skier through the ice
of 1 °C water floats on about 23 kg of trapped air, soaks about 9 kg into his clothes, has his
eyes under for good after about two and a half minutes lying still (five and a half swimming
up) and is metres down a minute after his breath is gone. Swim failure comes at
`failure_time(T)`: ten minutes at 0 °C, about half an hour at 5, nearly two hours at 10.

## Colour (`engine/water/optics`, M24e)

**Water's colour is what is in it**, so a look is stated as constituents and the colours are
derived, once, from the physics of light in water. Both the surface and the view from below read
the same coefficients, so a lake's colour from the shore and its fog under water cannot drift
apart.

| Constituent | Does | Stated as |
| --- | --- | --- |
| Pure water | Absorbs red strongly and blue barely: the blue of deep clear water | Built in (absorption per band from the measured spectrum: about 0.35 /m in the red, 0.06 /m in the green, 0.01 /m in the blue) |
| Dissolved organic matter (the brown) | Absorbs blue hardest, falling off exponentially toward the red: tea-coloured forest lakes | Its absorption at 440 nm, a_g(440), /m, and the slope (about 0.015 /nm) |
| Phytoplankton (the green) | Absorbs blue and red, scatters: green water | Chlorophyll, mg/m³ |
| Suspended sediment (the milk) | Scatters strongly, absorbs a little: glacial turquoise, a muddy river | Mineral mg/L and its tint |
| The bed | What shallow water shows through it: white sand gives turquoise | Albedo and the depth it lies at |
| Bubbles | White in surf and below a waterfall | From the foam and the flow |

Everything is computed spectrally, every 10 nm from 400 to 700 nm, and only then made a colour
(the CIE 1931 observer under daylight, in linear sRGB). The absorption a, the scattering b and
the backscattering bb are the sums of the parts above, with c = a + b, and a strongly forward
phase function (Henyey–Greenstein, g ≈ 0.9) for the particles. Daylight dims under the surface
by the diffuse attenuation Kd (from a and bb, the sun 30° up). The water-leaving colour over a
bed at a depth comes from a semi-analytic shallow-water reflectance model: the deep water's own
reflectance, set by u = bb / (a + bb), blended to the bed's albedo as the light's two paths
through the depth attenuate. Under water a view looks through a veil of the water's own
upwelling colour, a target fading into it as exp(−c·d), and a black target is lost at 4.8 / c
(c weighted by the eye's photopic response). The renderer reads `water::Optics`: each
coefficient averaged over a red, a green and a blue band, the deep colour, the veil, the bed and
the visibility. A body's optics are derived once, when it is laid.

Presets, each a row of constituents (not of colours): `"nordic-brown"` (a forest lake rich in
dissolved organic matter, a_g(440) of several per metre: amber shallows, black depths),
`"nordic-clear"` (a mountain lake above the tree line), `"baltic"` (green-brown brackish),
`"adriatic"` (an oligotrophic sea, clear and deep blue), `"tropical-shallows"` (low everything
over white carbonate sand: turquoise), `"glacial"` (rock flour: milky turquoise),
`"algal-pond"`, `"muddy-river"`, `"pool"`, and the brush world's `"slime"` and `"lava"` (lava is
not water: an emissive look on the same body kind, whose damage is the game's). A game states its
own water as a `water::Constituents` on the body's `Stuff::water`. Each named look carries the
band of visibility such water has in the field, and the `water-colour` lab fails a look whose
derived visibility leaves it:

| Look | Visibility (m) | Field band (m) |
| --- | --- | --- |
| `nordic-brown` | 1.58 | 0.5–3 |
| `nordic-clear` | 8.6 | 5–15 |
| `baltic` | 2.58 | 1.5–6 |
| `adriatic` | 23.3 | 15–40 |
| `tropical-shallows` | 21.9 | 15–40 |
| `glacial` | 2.75 | 1–5 |
| `algal-pond` | 0.45 | 0.2–1 |
| `muddy-river` | 0.12 | 0.05–0.3 |
| `pool` | 41.4 | 25–80 |
| `slime` | 0.13 | 0.02–0.3 |
| `lava` | 0.05 | 0–0.1 |

## Flowing water (`engine/water/flow`, M24f)

**Two tiers.** A **channel** (§ Bodies) is a river or a stream as a steady profile: cheap,
enough for most of a map. A **flow region** is simulated: the shallow-water equations on a
uniform grid over a bed, for water that has somewhere new to go.

- **The scheme**: a finite-volume Godunov scheme: each face's flux of mass and momentum by the
  HLL Riemann solver over the hydrostatic reconstruction (each side's depth taken over the
  face's higher bed, with the matching pressure correction), so still water over any bed stays
  still to rounding, wet fronts run onto dry cells at their characteristic speed, and the
  momentum is carried with the water (a front is not held at √(g·h)). Manning's friction and a
  game's own loss term act implicitly. Sub-steps keep the CFL condition at 0.4
  (Δt ≤ 0.4 Δx / (|u| + √(g·h))).
- **Exact mass**: a cell's water is an integer count of quanta (10⁻⁹ m³); each face's flux
  becomes quanta, a cell's outflow is scaled down to what it holds, and the same quanta leave one
  cell and enter the next, so nothing is made or lost in any pass. The books (start + added −
  drained = now) balance to the quantum in every lab and test.
- **Threads**: the passes run on bands of rows (`core::parallel_bands`); each pass reads only
  the state the previous pass finished, so one thread and eight give the same bits
  (`FlowSpec::workers`).
- **The bed** is a height per cell, from a heightfield or a brush world rasterised to column
  heights; walls are bed raised to their top. **Breaking a wall is lowering its cells**: the
  water behind it pours through, the basin empties at the rate its head allows, and the flood
  spreads over whatever is downhill. A lake or a volume woken into a region starts from its
  level over the bed.
- **Sources and sinks**: springs and inflows with a discharge (`Source`), drains with a hole's
  area and discharge coefficient emptying by Torricelli's law (`Drain`), and edges left open
  (`FlowSpec::open`, a bitmask of the four sides) that let the water out of the region; every
  other edge is a wall.
- **Waterfalls**: where the flow leaves a lip (the bed dropping much more than the depth, at
  speed), the water leaves the grid as a sheet on a ballistic path from the lip's velocity and
  re-enters where the path lands, after its time in the air, so the mass is held across the
  fall. The sheet's path, width and discharge are what the renderer's curtain and mist read.
- **Coupling**: the region's surface and velocity are what `sample` returns inside it, so
  floats drift on the current and swimmers are swept.
- **Limits** (stated, not hidden): it is 2.5D. One water column per cell: no water over water,
  no flooded overhang; a building's floors are separate regions joined by sources and sinks
  where water falls through a stair or a hole.

The checks (`tests/water_flow_test.cpp`, the `flow` lab), measured:

| Check | Law | Measured |
| --- | --- | --- |
| Dam break on a dry bed | Ritter's analytic solution | 0.015 m rms at 5 s, 0.011 at 10 s, 0.005 at 20 s; the front's thin tip at 206 m against 225 m at 20 s (a first-order scheme smears the tip) |
| A tank's drain | Torricelli, Q = Cd A √(2 g h) | within 1% at every head |
| A breach in a basin's wall | the weir, Q = C b H^1.5 | the power 1.49, the coefficient 0.78 of the broad-crested weir's |
| A river over a 6 m cliff | the sheet's ballistic arc | 2.00 m³/s in, 2.00 m³/s carried by the sheets, landing 13.75 m out (1.75 m beyond the lip) |
| Still water over a rough bed | at rest | speeds under 10⁻⁶ m/s after 10 s |
| The books | start + added − drained = now | to the quantum, through springs, drains, open edges, falls and tier changes |
| Threads | one against eight | the same bits |

## Drawn (`engine/render/water*`, M24g–M24i)

**Built (M24g, `engine/render/water.hpp`).** Two passes after the opaque scene, the sky and the
cloud, before the particles and TAA-U: `water.copy` copies the frame's colour and depth (the
water reads what lies behind it), and `water` draws every body, testing and writing the depth.
With no body shown both rows run empty. What it draws:

- **Geometry**: `WaterDraw::levels` nested squares of 64 quads round the eye, each snapped to
  twice its spacing and morphed into the next at its seam, the last level's rim pushed out to
  the horizon, every vertex clamped into the body's box; a lake's shore is where the ground
  stands over the water, feathered over its last 15 cm of depth.
- **Waves**: the simulation's own components in the same second-order formula, so the drawn
  crest is the one a float rides (the `gpu-water` lab reads it back: 0.8133 m drawn against
  0.8139 m simulated). A vertex keeps a component at least four of its level's spacings long;
  a pixel sums the slopes of every component (the simulation's, then 32 detail components from
  the band's top to 4 cm off the same spectrum, `detail_waves`) at least four pixels long.
- **The light**: Fresnel (Schlick, 2 %) between the mirror and the water seen into. The mirror
  is the opaque scene along the reflected ray, marched in screen space through the depth copy
  (a march that leaves a side goes on along the edge's column), else the sky's own model. The
  water seen into is the scene behind, bent by the normal, dimmed over the view's path and the
  light's way down to the bed by the body's optics, over the light the water itself sends
  back. The sun's glitter is Beckmann over every slope the pixel cannot resolve (the components
  it dropped, the spectrum's tail past the detail band, a calm surface's own ripples), so the
  far water glints as a broad path and never shimmers. A body's own glow (lava) is added.
- **In a game**: Fall Line's frame owns one `Water` and takes bodies by `add_water`; the `lake`
  lab digs a tarn into a seed's map to show it until the generator lays lakes (M24l).

Not built yet (M24g2): the GPU detail FFT and its cascades, foam, caustics, the shore's wet band,
rain, the lake shore mesh and the channel's and flow region's own geometry, the reflections over
the Hi-Z pyramid with a probe behind them, and the stops on the quality ladder. The plan for all
of it follows.

The water is a pass in the frame's table after the opaque geometry (it reads the scene's
colour and depth) and before TAA-U. All of it runs on GPU-less Linux on the software adapter,
so every view is a lab picture.

**The surface's geometry.** The ocean: nested square rings (the clipmap's layout) whose
lattice is snapped to the coarsest cell, so no vertex slides over the waves as the camera moves
and the surface does not swim; the far rings run to the horizon. A lake: its shore
triangulated once and the waves per pixel (a lake of square kilometres is a few hundred
triangles). A channel: a ribbon down its centreline. A flow region: its grid, heights uploaded
each step.

**The material**, per pixel:

- **Reflection**: Fresnel (Schlick at n = 1.333: 2 % looking straight down, nearly all at
  grazing) on the wave's normal; screen-space reflections over the Hi-Z pyramid, falling back
  to the sky and the far scenery from the frame's sky tables and the horizon (a calm lake
  mirrors its mountain).
- **The sun's glitter** at every distance: the slope variance of the waves too small for the
  mesh or the pixel is folded into the specular's roughness (filtered from the spectrum), so the
  far sea glints as a broad path instead of shimmering, which also keeps TAA-U steady.
- **Refraction and the body's colour**: the scene behind the surface bent by the normal, dimmed
  and tinted by the optics over the path's length through the water, plus the light the water
  scatters itself (§ Colour); light through a thin crest with the sun behind it.
- **Foam**: whitecaps from the Jacobian (the surface folding), kept and decaying in a texture
  so a crest leaves a trail; surf by the depth and the breaking share; foam where flow diverges
  and below a fall; as lace (a tiled pattern read to the share, with holes), never a white
  sheet.
- **Caustics** on the bed and on anything under the surface: the light focused by the waves,
  computed each frame from the surface's normals (the refracted grid's area ratio).
- **The shore**: the bed darkened where the waves run up and wet it, and the water
  feathered into the ground by depth so no edge is a contour line.
- **Rain** rings and a rougher surface under a shower.

**Moving water** (M24i): rivers' normals scrolled by a flow map in two phases (so the texture
never stretches), faster mid-stream; the waterfall's curtain along the sheet's path with
streaks running down it, mist and spray from the engine's particle pool, foam where it lands;
splashes from the splash events; wakes.

**Built (M24i, `engine/render/water_stream.hpp`).** The water pass draws a body's wake rings
with its waves, each kept by the vertex and the pixel the way a wave is. After the still bodies
it draws the moving water, built on the CPU each frame from the simulation: a channel's ribbon
(nine vertices bank to bank every metre down its line, on the surface `sample` reports), a flow
region's wet cells, and every falling sheet as a curtain along its ballistic path from the lip to
where it lands. The surface's ripples are read through a flow map in two phases (1.6 s a cycle),
rougher where the water runs faster; foam is lace cut to a share that rises where the water runs
fast and shallow, where the flow is supercritical and round where a sheet lands. The curtain is
aerated white with streaks running down it at the sheet's speed, lit as a face tipped up to the
sky so it reads white with the sun behind it. `waterfall_spray` throws about 60 drops a second for
every m³/s landing (six in ten a fine mist that drifts, the rest spray), and `splash_spray` a
crown of drops for a splash, both into the particle pool (`render/particles.hpp`) off the
caller's `core::Rng`. The still and the moving water share one light,
`render/wgsl/water_light.wgsl`'s `shade_water`.

**Built (M24h, `engine/render/underwater.hpp`).** When the eye is under a body's level and inside
its box (`Water::eye_under`), the `water.under` pass redraws the frame from the water's copies:
the scene fades by the body's beam attenuation into its veil (`Optics::veil`, lit by the day
dimmed over the water above the eye), what stands under the surface lit by the day dimmed on its
way down; the bare eye's blur by distance; a ray that meets the surface first sees the world above
within Snell's window (48.6 degrees round straight up, measured 48.9 off the `gpu-underwater`
lab's picture) and the water mirrored past it; the eye adapts to the dark (the picture lifted up to
six times so the veil reaches `adapt`); the wobble and a flat tint. The four presets below are
`underwater_look`'s; a game sets `Underwater::look` (Fall Line's frame owns one). The world above
is seen through the window unbent: the screen holds only what the camera saw, and the low
horizon a real window squeezes into its rim lies off it.

**Built (M24h2, `engine/render/underwater.hpp`, `engine/render/wet_lens.hpp`).** THE WATERLINE:
the lens is a plate `waterline` m tall (0.12) across the screen's height, so an eye within that of
the surface, under it (`eye_under`) or just over it (`Water::eye_near`, within
`kWaterlineBand`), sees the picture split where the plate crosses the water. Each pixel's point on
the plate is tested against the surface there: the plane through the surface at the eye tilted by
its slope (`Waterline`, the caller's `water::sample` at the eye, rings and crater included, or
the flat level), lapping a centimetre; under it the view under water from that point's depth, over
it the frame as drawn, and a dark meniscus a few pixels wide on the line; the pass blends by the
share under, so the dry pixels are left alone. THE LIGHT IN THE WATER comes from above: the veil
is brighter looking up than down (three times, between the nadir and the zenith). THE SUSPENDED
MATTER: specks fixed in the water, drifting a little, found per pixel on six shells round the eye
from 0.1 to 1.6 m (each a hashed grid of cells holding at most one speck, faded in and out as the
shell moves through it, so none pops), lit by the light in the water and fogged over their own
distance; their share is the look's (`motes`) times the water's beam attenuation over 2 per metre,
so a brown lake or an algal pond is thick with them and a pool has next to none. THE WET LENS
(`render/wet_lens.hpp`): leaving the water, a sheet covers the lens and its top edge runs down the
screen over half a second, accelerating, the view under it rippled; it leaves 40 drops as it
passes, more small than big; a drop over 0.02 screen heights slides, faster the bigger, and leaves
beads behind it; every drop dries over its life (5 s for the biggest), and a splash near a dry
lens spatters it. The drops are the CPU's (`step_wet_lens`, off its own `core::Rng`), so a lab
digests them and a replay shows the same ones. The pass (`water.lens`) runs after TAA-U at the
output's size, each drop a lens of water showing the scene behind it upside down and out of focus
with a dark rim and a highlight; post reads its target only while the lens is wet.

Not built yet (M24h3): light shafts, caustics on what is under the surface, the world above bent
into the rim of Snell's window. The detail waves' interference shows as a regular hatch in the
glitter a few centimetres under an eye at the surface; the GPU detail FFT (M24g2) replaces them.

**Under water**, the plan: one `UnderwaterLook`, derived from the body's optics and open to
every knob a game wants:

| Knob | What it does |
| --- | --- |
| Absorption and scattering | The fog: the scene fading into the water's own colour with distance, red first; the visibility in metres |
| Blur | The eye in water loses most of its focusing power, so the bare-eyed view is blurred by distance; a mask or goggles sharpen it (and magnify by about a third) |
| Distortion | A slow refraction wobble |
| Light shafts | The sun's rays through the surface, masked by the caustics |
| Caustics | On everything under the surface |
| Snell's window | Looking up, the sky only within about 48.6° of straight up; beyond it, the surface mirrors the water below |
| The waterline | A camera half in: the image split along the surface's line with a meniscus; drops on the lens on leaving the water |
| Particles | Suspended matter drifting by, thick in murky water |
| Tint and warp overrides | A stylised look (the classic first-person game's flat tint and screen warp) |

Presets: `"eyes-open"`, `"mask"`, `"murky"`, `"classic"`.

**The quality ladder** (M8f4): the FFT's size and cascades, the reflections (SSR or the probe
alone), the caustics, the ripple field, the foam's history each have stops and prices like
every other picture row.

## Level of detail: simulate and draw only what is close enough to matter

Water a player cannot reach and cannot see the waves of costs nothing to simulate, and the
water at his feet is where the picture is judged. So both halves run in tiers by distance, and
the one rule that keeps the game deterministic decides **what the distance is measured from**:

- **The simulation's tiers are measured from the game's own state**, never from the camera: from
  every **actor** the game names (each player, each floating body that is moving, each swimmer,
  a breach that has just opened). A camera is a machine's and differs between a server, its
  clients and a replay; a player's position is the same on all of them. In a single-player game
  the two are a few metres apart, so nothing is lost.
- **The picture's tiers are measured from the camera**, and nothing the picture does is read
  back by the simulation.

| Tier | Simulation (from the nearest actor) | Picture (from the camera) |
| --- | --- | --- |
| **Near** (about 0–60 m) | Every wave component and every wake; a flow region at its full cell, every step; floats and swimmers on all their probes | The finest rings; the simulation's components, all three FFT cascades and the ripple field as geometry and normals; caustics, foam with history, splashes and spray, refraction, SSR |
| **Mid** (about 60–400 m) | The waves are a pure function, so they are only ever evaluated where asked and cost nothing here; wakes past their decay radius dropped; a flow region's cells merged 2×2 (and 4×4 farther) and stepped at half the rate; a float with no actor near **sleeps** at its rest pose on the analytic surface | Coarser rings; the FFT cascades fading out one by one as their wavelength drops under a pixel, their slopes folded into the specular's roughness; Jacobian foam; no ripples, no caustics, no particles |
| **Far** (beyond about 400 m) | A settled flow region **sleeps** (its state held, costing nothing until an actor or a breach wakes it); a channel's steady profile stands in for a far stretch of simulated river; nothing else | Only the long components as geometry; every shorter wave folded into the lighting (the filtered slope variance), so the far water glints right and never shimmers; the sky's reflection, no SSR |
| **Horizon** | Nothing | The sea drawn analytically to the horizon in the sky pass's terms, matching the far rings' colour at their rim |

**Changing tier never pops and never loses water.** A flow region coarsened merges each 2×2
block of cells by summing its quanta and averaging its bed and momentum; each coarse face keeps
the crest of the finer faces along it, so a wall thinner than a coarse cell still holds its
water. Refined, each coarse cell's water is levelled over its four finer beds (lowest first)
to the quantum. The finest bed is always kept, so at a coarse tier `flow_at` stands the coarse
surface over the true bed (a rock inside a coarse cell stays dry). Measured in `water-lod`, a
flood changed tier six times in 12 s holds its water to the quantum, and the surface moves by
a median of 1–3 cm (95th percentile 2–16 cm); the largest jumps, up to 2.5 m, are on the cells
beside a 6 m wall, the relief a coarse cell cannot see. `tier_for(flow, actors, near, mid,
band)` picks a region's tier from the nearest actor, moving each edge out by `band` while the
region is inside it (hysteresis), so an actor wobbling on a boundary does not flicker it. A sleeping float wakes exactly on
the analytic surface it slept on. On the picture's side each cascade and each effect is faded
by distance over its band rather than switched, and the `water-lod` lab's per-distance-band
picture diff holds it.

**The tiers' distances are a game's to set** (a shooter's room is all near, a mountain's
valley lake is mostly far, a watercraft racer's sea stretches to the horizon) and the quality
ladder scales the picture's: a phone's near tier is shorter than a desktop's.

## Sound (`engine/water/sound`, M24j)

Voices built on the synth, driven by the water's own numbers: lapping (noise bands by the
spectrum's height and period, louder at a shore), a river's rush (by discharge and speed), a
waterfall's roar (by the drop and the discharge, low and wide), splashes by the event's speed
and size, a swimmer's strokes and breaths, and the world muffled under water (a low-pass and
a damped room) with bubbles.

**Built** (`engine/water/sound.hpp`, on the synth in `engine/audio`). **What is heard** is one
function, `hear`: it asks `sample` at the ear and on seven rings round it (1.5 to 96 m, each
twice the last, eight directions), so the sound can never disagree with the physics or the
picture about where the water is, and adds the flow regions' sheets. Each voice is an
INTENSITY at the ear, the power the water spends where it is over the square of the distance:

- **the lapping**: the waves' energy, rho g (the whole sea's variance Hs^2 / 16 from
  `Sample::hs`, the laid crests rising over it as they pass), spent in full where the water is
  shallower than 1.2 m or the next ring out is dry (the shore) and at a twelfth on open water;
  its **surf** is the breaking share of it (`Sample::breaking`);
- **a river's rush**: the bed's friction, rho c_f |v|^3 with c_f = 0.004, over a channel's or a
  flow region's wet points, so a mountain stream is far louder than a slow deep river of the
  same discharge, as it is;
- **a waterfall's roar**: each sheet's fall, rho g Q h, at the distance to where it lands, over
  2 pi r^2: twice as far is half as loud (the `water-audition` lab's chart).

A ring's points each carry the share of an even sheet of water their sector has, the same on
every ring, because the area grows as r^2 as the intensity falls as 1 / r^2. A level is the root
of the intensity over its voice's full one (`kFull*`), held at 1.5. Each voice also carries
where it comes from (the pan, by the ear's heading) and its intensity-weighted distance, with
which the air takes the highs.

**The water bed** is seven noise layers on a rack, a pure function of the hearing
(`water_targets`): the lap (pink, a band falling with the waves' period), the surf (a high
pink band), the rush (pink, brighter with the current) and its babble (a narrow white band),
the roar (brown, darker the bigger the drop), the spray (the drops landing round the ear: only
near the foot) and the room under water. **Under water** (the ear more than 0.4 m down) the
water's own voices keep their level and lose their highs to 500 Hz, and the room (a low brown
hum, louder near a fall) comes up; `muffle` pulls a game's AIRBORNE layer or one-shot down to
420 Hz and a quarter of its level, so the whole world goes under together.

**Bubbles** ring at their Minnaert frequency, f r = 3.26 m/s at the surface's pressure (a 3 mm
bubble at 1.09 kHz), a sine rising a little as it nears the surface. They are a river's babble
(1.5 to 5 mm, above water), the churn under a fall and round a swimmer under water (2 to 12 mm),
and the cloud a splash takes down. **Splashes** (`play_splash` off a `Splash`): the crack and
the drops falling back, pitched down and stretched as the body grows, and the cavity's whump
(`plunge`) growing faster than the crack, so a pebble plips and a boulder booms. **A swimmer**
(`play_swim` off `SwimEvents`): the splash in and out, a stroke (the pull, the drops off the
arm, the hand going in), a breath every other stroke with his eyes out, a few bubbles from a
stroke under, and the gasp coming up.

Not yet: a fall heard through the rock between (the rings know water, not ground); the room's
own echo under water (the synth's echo is one room for every voice); rain on the water.

## Adding water is one call (M24k)

Built. A body is one call, the step is one call and every question is one call:

```cpp
water::World world(wind_field);                               // the air the waves grow from
water::add_lake(world, shore_outline, /*level*/ 412.0, {.look = "nordic-clear"}, bed);
water::add_river(world, centreline, /*discharge m³/s*/ 6.0, {.look = "glacial"});
// each step:   water::step(world, dt);            (the flow regions; a lake costs nothing)
// any query:   water::sample(world, x, y, z, t);
// each frame:  the renderer and the sound (water_bed_update) read the same world
```

`water::step` steps everything in the world that is stepped, which today is the flow regions
(the lakes, oceans, channels and volumes are pure functions of the time, and the wakes prune
themselves as they are emitted); a game's floating bodies and swimmers stay its own.

**In a game repo, `ge add water [<name>]`** (default `water`) writes it from
`templates/water/` and links `ge::water` into the game's simulation:

| File | What |
| --- | --- |
| `sim/<name>.hpp`, `.cpp` | `make_<name>(wind)`: a lake about 85 m by 55 m on a 12-point shore, its bed a bowl 6 m deep shelving to 0.3 m at the shore, the look `nordic-clear`, the wind from +z; its waves laid down to 0.25 m (`World::lay.shortest`), since a small lake's sea is shorter than the default's 1.5 m and would otherwise lay no component at all |
| `labs/<name>_lab.cpp` | A picture lab, three shots: the lake from above, every pixel one `sample` (the look's water-leaving colour over the bed at that depth, white where the waves break); 6 m by 8 m of the windward and of the lee water side by side at 25 px a metre, shaded by the waves' normal; and a section across it (the surface over the bed, the waves drawn forty times over). The numbers: the sea's height at the lee and the windward water, the depth, the look's visibility |
| `tests/sim_<name>_test.cpp` | Wet and deep in the middle and dry past the shore, a sample's bits replayed, the lee shore rougher than the windward one |

It syncs the `water` consumer skill (`plugin: water`: a repo gets it once it links
`ge::water`), which tells a downstream agent how to lay each kind, choose a look, float a hull,
pick a swimmer, break a wall into a flow region and hear it. The scaffold's lab draws on the CPU
raster: the engine's GPU layer is not published to game repos yet, so the drawn water a game
gets is the engine's own `gpu-water` once it is. Packaging's `scaffold` job grows a repo with
`ge add water` and holds it to `ge check`, its tests and its lab's smoke pass.

Not yet: waking a lake or a volume into a flow region when the game breaks what held it (a game
lays the region with `add_flow` and `fill_to` today); a window of the GPU's water in a game repo.

## Determinism (G1)

Wind, waves, channels and optics are pure functions of their data and the time: nothing to
step, nothing to send over the network, the same bits on every machine. Floating bodies and
flow regions are stepped state, in doubles, `core/libm` only, with a fixed step and a fixed
order, held by golden tables; a flow region's state is the game's to snapshot. Everything
the GPU adds (the FFT detail, the ripples, the foam's history, the particles) is drawn only and
never read back by the simulation.

## Labs (G9)

| Lab | Kind | Row | What it shows |
| --- | --- | --- | --- |
| `wind` | picture | M24a | The gust series for three grounds with the intensity asked and measured and the gust factor, the log profile over each ground, a sheltered field's plan with a downwash and a whirl, the sources' decay (the mountain's wind is held bit for bit by its golden) |
| `waves` | picture | M24b | The spectrum for a sweep of winds and fetches against the growth laws, the components laid, the height series at a point (measured Hs and Tp against the asked ones), plan maps of the surface, the breaking shares |
| `fetch` | picture | M24b | A body's fetch and its sea per point, for a lake, a fjord, a coast |
| `float` | scenario | M24c, M24i | A box, a log, a ball, a hull and a sled skipping: immersion, heave and its period against theory, the drift on a current; a hull's wake in plan at two speeds with Kelvin's crest lines over it, and its arms' angle measured at four speeds |
| `swim` | scenario | M24d | Every preset side by side: the classic's scripted trajectories, the idle sink, the jump out; the clothed skier's sink over minutes, the cold's stages; the default swimmer |
| `water-colour` | picture | M24e | Every look's swatches over depth and bed, in sun and shade, from above and below; visibility per look |
| `flow` | picture | M24f | Dam break against the analytic solution, a basin emptying through a breach against Torricelli, a river to steady state, a waterfall's sheet; depth filmstrips; the volume held |
| `gpu-water` | picture | M24g, M24g2, M24i | Named views: the calm mirror, a windy lake, the sea at several Beaufort forces, surf on a beach, a boat's wake from behind and from the side, crates dropped in, a river, a waterfall, sunset glitter, night, rain; the distance-band diff |
| `lake` | picture | M24g, M24h, M24l | Fall Line's own frame with a tarn on a seed: from the shore, from over it, into the sun, in a fresh wind, at the day's end, under it and looking up from under it |
| `gpu-underwater` | picture | M24h, M24h3 | Every look's view round a scene under water, the presets, Snell's window measured, a dive through the surface; then the caustics and the shafts |
| `gpu-plunge` | picture | M24h2 | A first-person player in the classic move jumping off a bank into a dirty pond: the plunge from his eyes, the splash from the bank (crater, crown, jet, rings), under the water, surfacing through the waterline into the wet lens; four bodies of different mass dropped in; the entry's numbers |
| `water-lod` | picture | M24f, M24g | The tiers round an actor and round the camera as maps; a flow region coarsened and refined with its volume held; the picture diff per distance band across a slow fly-out (no band pops); the cost against distance |
| `water-audition` | sound | M24j | Every water voice, metered and written as WAV: the bank, splashes from a pebble to a boulder, eleven places (a calm and a fresh lake from the shore, a boat and under it, a gale on a beach, a slow river and a stream, a waterfall from 15 and 60 m and under its pool, nowhere), the roar against distance beside 1 / r, a swim |
| `water-cost` | bench | M24f, M24g | The CPU's costs per step (sample, floats, the flow grid) and the GPU's per pass |
| `free --water` | drive | M24l | Fall Line: the bot near and into water on the free ride |

## Sources

The numbers above come from: the fetch-limited spectrum (Hasselmann et al. 1973, JONSWAP),
the fully developed sea (Pierson and Moskowitz 1964), the depth correction (Bouws et al. 1985,
TMA), the fetch laws and the effective fetch (the US Army Shore Protection Manual, 1984),
linear wave theory and Stokes' second order, the statistical FFT ocean (Tessendorf, Simulating
Ocean Water, 2001), filtering wave slopes into the lighting (Bruneton, Neyret and Holzschuch
2010), flow maps in two phases (Vlachos 2010), the flux form of the shallow-water equations
(O'Brien and Hodgins 1995; Mei, Decaudin and Hu 2007) and their validation cases (Ritter's dam
break, Torricelli's law), planing (Savitsky 1964), the absorption of pure water (Pope and Fry
1997), dissolved organic matter and phytoplankton (Bricaud et al. 1981, 1995), particle
scattering (Gordon and Morel), shallow-water reflectance (Maritorena et al. 1994; Lee et al.
1998), and cold-water immersion (Tipton; Golden and Tipton, Essentials of Sea Survival).

## Not taken

- **A height-field grid for splashes and ripples.** The common cheap answer to a body falling in is
  a small grid of heights round it stepped by the wave equation (or by a convolution kernel that
  disperses each wavelength at its own speed). It costs a grid and a step for every splash every
  frame, needs absorbing edges, and its shortest waves are its cells. The rings are evaluated only
  where a vertex or a pixel asks, already disperse every length at its own speed, and the near
  field a grid would capture (the cavity and its jet) is one closed form on the same ring
  (`entry.hpp`).
- **SPH or FLIP for the simulation.** Full 3D particle fluids are the right tool for a glass of
  water and the wrong one for a lake: their cost grows with the volume, they are hard to make
  bit-deterministic across threads, and a game's water is almost all surface. The 2.5D grid
  takes the floods, breaches and rivers; particles are used only for what is drawn (spray, mist,
  splashes).
- **FFT for the simulation.** A deterministic CPU FFT would work, but a probe needs the height
  at a point, not a tile, and 16–32 components evaluated where asked cost less than a tile per
  step and need no inversion of the choppy displacement.
- **Gerstner waves for the simulation.** Their horizontal displacement makes the height at a
  point an inversion; the second-order form gives the same crest shape as a plain function.
- **Planar reflections.** A second render of the world for each plane; screen-space
  reflections with the sky and horizon behind them cost a fraction and work on waves.

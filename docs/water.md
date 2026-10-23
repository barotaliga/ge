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

A `WaterWorld` holds the water a game has laid down. **Every question anything asks of water
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
  part laid as 16–32 components, cut into equal-energy slices (they crowd onto the peak,
  so the peak is a wave train, not one sine beating against an octave-off neighbour) and their
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
    swimming is weak in a soaked suit (drag area up, stroke thrust down). Skis come off and
    float on their own.
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

Per colour band the absorption a and the scattering b give the extinction c = a + b, the
single-scattering albedo and a strongly forward phase function (Henyey–Greenstein, g ≈ 0.9 for
particles). The water-leaving colour over a bed at a depth comes from a two-flow shallow-water
reflectance model (the deep water's own colour blended to the bed's by exp(−2·K·depth)), and the
visibility under water from c (a target vanishes at about 4.6 / c).

Presets, each a row of constituents (not of colours): `"nordic-brown"` (a forest lake rich in
dissolved organic matter, a_g(440) of several per metre: amber shallows, black depths),
`"nordic-clear"` (a mountain lake above the tree line), `"baltic"` (green-brown brackish),
`"adriatic"` (an oligotrophic sea, clear and deep blue), `"tropical-shallows"` (low everything
over white carbonate sand: turquoise), `"glacial"` (rock flour: milky turquoise),
`"algal-pond"`, `"muddy-river"`, `"pool"`, and the brush world's `"slime"` and `"lava"` (lava is
not water: an emissive look on the same body kind, whose damage is the game's). A game may add
rows; a tint override exists for art direction but is never the default.

## Flowing water (`engine/water/flow`, M24f)

**Two tiers.** A **channel** (§ Bodies) is a river or a stream as a steady profile: cheap,
enough for most of a map. A **flow region** is simulated: the shallow-water equations on a
uniform grid over a bed, for water that has somewhere new to go.

- **The scheme**: the flux (virtual pipe) form: each cell's depth and the flux through its four
  faces, accelerated by the difference in surface height across each face, limited so no cell
  drains below empty, the depths updated from the net flux. It conserves the mass exactly,
  handles dry cells and wetting fronts, and is cheap enough for 512² cells on the CPU at the
  fixed step, split into rows across threads with every row reading only the previous state, so
  the result is the same bits on any thread count. Sub-steps keep the CFL condition
  (Δt ≤ Δx / (|u| + √(g·h))).
- **The bed** is a height per cell, from a heightfield or a brush world rasterised to column
  heights; walls are bed raised to their top. **Breaking a wall is lowering its cells**: the
  water behind it pours through, the basin empties at the rate its head allows, and the flood
  spreads over whatever is downhill. A lake or a volume woken into a region starts from its
  level over the bed.
- **Sources and sinks**: springs, inflows with a discharge, drains, outflows at the region's
  edge, rain.
- **Waterfalls**: where the flow leaves a lip (the bed dropping much more than the depth, at
  speed), the water leaves the grid as a sheet on a ballistic path from the lip's velocity and
  re-enters where the path lands, after its time in the air, so the mass is held across the
  fall. The sheet's path, width and discharge are what the renderer's curtain and mist read.
- **Coupling**: the region's surface and velocity are what `sample` returns inside it, so
  floats drift on the current and swimmers are swept.
- **Limits** (stated, not hidden): it is 2.5D. One water column per cell: no water over water,
  no flooded overhang; a building's floors are separate regions joined by sources and sinks
  where water falls through a stair or a hole.

The checks: a dam break against Ritter's analytic solution for a dry bed; a basin emptying
through an orifice against Torricelli's law (the outflow ∝ √head); a steady river's
discharge in equals its discharge out; the total volume constant to the bit when there are no
sources or sinks.

## Drawn (`engine/render/water*`, M24g–M24i)

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

**Under water** (M24h): one `UnderwaterLook`, derived from the body's optics and open to
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

**Changing tier never pops and never loses water.** A flow region coarsened merges cells by
their volume and is refined by spreading each coarse cell's volume over its fine cells along
the surface's slope, so the mass is held to the bit and the surface moves by less than the
coarse cell's own error; tiers overlap by a band and a cell changes tier only past it
(hysteresis), so an actor on a boundary does not flicker it. A sleeping float wakes exactly on
the analytic surface it slept on. On the picture's side each cascade and each effect is faded
by distance over its band rather than switched, and the `water-lod` lab's per-distance-band
picture diff holds it.

**The tiers' distances are a game's to set** (a shooter's room is all near, a mountain's
valley lake is mostly far, a watercraft racer's sea stretches to the horizon) and the quality
ladder scales the picture's: a phone's near tier is shorter than a desktop's.

## Sound (`engine/audio`, M24j)

Voices built on the synth, driven by the water's own numbers: lapping (noise bands by the
spectrum's height and period, louder at a shore), a river's rush (by discharge and speed), a
waterfall's roar (by the drop and the discharge, low and wide), splashes by the event's speed
and size, a swimmer's strokes and breaths, and the world muffled under water (a low-pass and
a damped room) with bubbles.

## Adding water is one call (M24k)

```cpp
water::WaterWorld world;
water::add_lake(world, shore_outline, /*level*/ 412.0, water::look("nordic-clear"));
water::add_river(world, centreline, widths, /*discharge m³/s*/ 6.0, water::look("glacial"));
// each step:   water::step(world, wind, dt);       (flow regions and floats only)
// any query:   water::sample(world, p, t);
// each frame:  render::WaterPass reads the world;  audio reads the same world
```

`ge add water` writes this into a game repo with a lab beside it, and the `water` consumer
skill tells a downstream agent how to choose a look, a swimmer and a flow region.

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
| `wind` | picture | M24a | The gust series and its spectrum, the log profile over each ground, a sheltered field's plan with arrows, local sources; the mountain's wind against its own function, bit for bit |
| `waves` | picture | M24b | The spectrum for a sweep of winds and fetches against the growth laws, the components laid, the height series at a point (measured Hs and Tp against the asked ones), plan maps of the surface, the breaking shares |
| `fetch` | picture | M24b | A body's fetch and its sea per point, for a lake, a fjord, a coast |
| `float` | scenario | M24c | A box, a log, a ball, a hull and a sled skipping: immersion, heave and its period against theory, the drift on a current |
| `swim` | scenario | M24d | Every preset side by side: the classic's scripted trajectories, the idle sink, the jump out; the clothed skier's sink over minutes, the cold's stages; the default swimmer |
| `water-colour` | picture | M24e | Every look's swatches over depth and bed, in sun and shade, from above and below; visibility per look |
| `flow` | picture | M24f | Dam break against the analytic solution, a basin emptying through a breach against Torricelli, a river to steady state, a waterfall's sheet; depth filmstrips; the volume held |
| `gpu-water` | picture | M24g, M24i | Named views: the calm mirror, a windy lake, the sea at several Beaufort forces, surf on a beach, a river, a waterfall, sunset glitter, night, rain; the distance-band diff |
| `gpu-underwater` | picture | M24h | Every look's view round a scene under water, Snell's window, the caustics, the waterline split |
| `water-lod` | picture | M24f, M24g | The tiers round an actor and round the camera as maps; a flow region coarsened and refined with its volume held; the picture diff per distance band across a slow fly-out (no band pops); the cost against distance |
| `water-audition` | sound | M24j | Every water voice, metered and written as WAV |
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

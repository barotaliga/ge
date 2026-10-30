# Thunderstorms, lightning and thunder

The engine's thunderstorms (M37) are pure functions of place and time in `engine/wind`, like the
wind they blow in: a `Storm` is laid once, and what its cells, its lightning and its thunder do at
any point and moment is computed from it, never stepped. A replay, a ghost, a lockstep peer and a
server read the same storm with nothing to send. This page states what a real thunderstorm does,
the numbers the engine takes from that, and how a game reads it.

| File | What |
| --- | --- |
| `engine/wind/storm.hpp` | Cells and their life cycle, the gust front (`outflow_at`), the rain (`rain_at`), a multicell storm (`lay_storm`) |
| `engine/wind/lightning.hpp` | The flash schedule, flash kinds, strokes, the light over a window, the channel |
| `engine/wind/thunder.hpp` | Thunder's arrival, envelope and cut-off at a listener, and its sound |
| `engine/lab/labs/storm_lab.cpp` | The `storm` lab |

## The cell

A thunderstorm is made of cells, each a convective updraft that lives for about an hour:

| Stage | How long | What happens | The engine |
| --- | --- | --- | --- |
| Towering (cumulus) | about 15 min | An updraft builds the tower; no rain reaches the ground yet; the first echo appears near its end | the updraft rises to 8 + 32 × peak m/s; the top climbs to 7 + 7 × peak km; the first rain at 0.7 of the stage |
| Mature | 15 to 30 min | The rain drags a downdraft down beside the updraft; the heaviest rain, the most lightning; the downdraft's cold air spreads out on the ground as a gust front; the top spreads into an anvil | 900 + 900 × peak s; the downdraft 4 + 16 × peak m/s; the rain at its core 0.45 + 0.55 × peak on the precipitation dial |
| Dissipating | about 30 min | The downdraft cuts off the updraft; the rain thins to the anvil's light fall; lightning dies away, a larger share of it to the ground | 1800 s; the rain falls to a third, then to nothing |

`peak` is the cell's strength: 0.3 a weak cell, 0.5 an ordinary thunderstorm (about 10.5 km tall,
about 6 flashes a minute), 1 a severe one (14 km, about 40 flashes a minute).

**Lightning starts** once the tower passes the glaciation level, where graupel and ice crystals
collide in the updraft and separate charge (about the -20 °C level, 6.5 km over the ground here).
The rate rises with the updraft and lags it on the way down; the engine's peak rate is
1 + 40 × peak³ a minute. The first flashes nearly all stay in the cloud.

**A multicell storm** renews itself: each cell's gust front lifts the warm inflow on its forward
flank, and a new cell rises there every 10 to 15 minutes. `lay_storm` places each new cell ahead of
where the last has drifted and to the right of the drift (0.7 rad), at its gust front's distance
plus 4 km, for as long as the storm is asked to renew.

## The gust front

The downdraft's rain-cooled air touches down as the cell matures and spreads over the ground as a
shallow ring (the cold pool). Its edge runs out at 4 + 8 × peak m/s from the cell. The air behind
it blows at 8 + 22 × peak m/s, strongest just behind the edge (the head, 600 m wide), and it is
carried on the cell's drift, so the forward side blows hardest. It is under 1.5 km deep, with its
strongest wind some tens of metres up and the log law below that. At the head it lifts the warm
air it undercuts, which is where a shelf cloud forms and the next cell rises. The wind is spent
within a quarter hour of the cell's decline, though the boundary keeps spreading. `outflow_at`
returns it as a `wind::Air` to add to the field's wind, its `gust` the nearness to a head.

## The rain

`rain_at` gives each cell's fall on the precipitation dial (`wind/precip.hpp`'s `intensity`). It is
the cell's rain at its core, falling off as 1 / (1 + (d / r)²)² with the distance d from it, the
core's radius r widening as the cell matures and as it rains out under the anvil. Cells combine as
independent chances, 1 - Π (1 - each). The freezing level under a downdraft is a world's to lower
through `rain_share_at`.

## Lightning

| Quantity | Observed | The engine |
| --- | --- | --- |
| Flashes that stay in the cloud | about 3 in 4 at mid-latitudes, more in severe storms | 0.7 + 0.2 × peak, 0.97 in the first minutes, 0.55 as the cell dies |
| Strokes in a ground flash | 3 to 5 on average, up to 10 or more | 1 + geometric (0.75 to go on), at most 12 |
| Single-stroke ground flashes | 15 to 30 % | about 25 % |
| Time between strokes | log-normal, geometric mean about 60 ms, 10 to a few hundred ms | log-normal about 60 ms, spread 0.7, 8 to 300 ms |
| Later strokes' brightness | a third to a half of the first's current | 0.25 to 0.75 of the first's light |
| Continuing current | 30 to 50 % of ground flashes, 40 ms or more | 40 % of flashes, 40 to 300 ms after one stroke |
| Stepped leader | 20 to 35 ms, faint | 15 to 40 ms |
| A whole flash | 0.2 to 1 s | the same |
| Ground flashes out of the anvil | a few % (a bolt from the blue) | 4 %, striking 8 to 15 km out ahead |

**The schedule.** Each live cell's clock is cut into 1 s slots. A slot holds a flash with the
chance the cell's rate gives it, hashed off the cell's seed and the slot, so any window lists the
same flashes whoever asks and in whatever pieces (`flashes_between`, `flash_in_slot`).

**The kinds.** A *cloud* flash (intra-cloud) runs between the main negative charge and the upper
positive one, 6 to 9 km up, 3 to 12 km long and mostly level, spreading most often downwind into
the anvil. Its light is a dim glow of 0.2 to 0.8 s broken by 3 to 10 recoil pulses (K-changes).
Seen through the cloud it lights the cloud from within: that is **sheet lightning**, which is not
its own kind of discharge. A flash too far away to be heard (past about 25 km) is **heat
lightning**. A *ground* flash (cloud-to-ground) leaves the lower charge region (4 to 6 km up)
under the core and strikes within about a core's radius of the cell's centre.

**The light.** A return stroke's light decays in about 2 ms, a pulse in the cloud in about 6 ms,
and a continuing current is a steady glow. `light_over(flash, a, b)` integrates all of it in
closed form over a window, so a frame of 1/60 or 1/120 s gets exactly the light that fell in it,
however short the stroke. The **flicker** the eye sees is the gaps between strokes, tens of
milliseconds apart and so often spanning frames, and the held glow of a continuing current. The
unit is the mean over a 1/60 s frame of a stroke of energy 1. `sky_light` brings every flash's
light to a viewer (half at 4 km) as one scalar for the frame; `brightest` gives the strongest few
as positioned lights, at the channel's middle with half its length as a radius.

**The channel** (`build_channel`). A stepped leader advances in steps of about 50 m, each bent
from the last (the channel's tortuosity, around 16 degrees on average) and branching downward.
The engine walks 60 m steps (80 m in the cloud) bent about 20 degrees a step, drawn toward the
strike point and pulled hard onto it in the last few steps. It branches about one step in 16 (one
in 10 in the cloud) and sub-branches on the branches, each branch fading along its length. The
first return stroke lights the branches as well as the main channel; the later strokes follow the
main channel only. A channel is at most 1024 segments, built into a caller's buffer.

## Thunder

A stroke heats its channel to about 30 000 K in microseconds, and the shock relaxes to sound
within metres. Every stretch of the channel is heard when its sound has come the distance, so:

- **The delay** is the distance over the speed of sound, 331.3 + 0.606 m/s per degree Celsius
  (343 m/s at 20 °C, about 3 s a kilometre).
- **The length** is the channel's spread in distance from the listener: a channel running away
  from the listener rumbles long; one broadside to it claps short.
- **The rumble's lumps** are the channel's bends and branches, which put more channel at some
  distances than others.
- **The air takes the highs**: the cut-off falls from a close crack's several kilohertz to under
  100 Hz at 15 km (the engine: 5000 / (1 + d / 300 m) Hz).
- **The air bends it up**: the air cooling with height refracts the sound over a distant listener,
  so thunder is rarely heard past 15 to 25 km (the engine fades it from 12 km to nothing at 25 km).

`hear_thunder` bins the arrival of every segment's sound from every stroke in 10 ms. Each bin's
intensity is the segment's length and glow over its distance squared, its cut-off the
intensity-weighted mean of the segments in it. `render_thunder` makes the sound: hashed noise
under that envelope, low-passed bin by bin at its cut-off, over a 70 Hz rumble that lingers on
the envelope, soft-clipped, mono at any rate.

## Thunder heard

`engine/wind/thunder_play.hpp` plays a storm's thunder on the synth's sampled voices, so a game
only has to say where the ear is:

- `hear_storm(player, storm, t, ear)` looks at every flash that started since the last call. The
  first call looks back as far as thunder carries (25 km at the speed of sound, about 75 s), so a
  listener arriving under a storm hears the thunder already on its way. For each flash it works
  out the thunder at the ear, renders it once, and queues it at its first sound, panned by the
  bearing to the channel's nearest point.
- `play_thunder(player, synth, t, block_s, bus)` starts every queued clap due inside the block,
  its first sample placed in the block by the play's delay. It frees a clap's recording once its
  voice has ended. A clap more than 0.25 s late (a run paused through it) is let go.

Rendering is the cost: a near strike is up to 60 s of samples, made once a flash. No more than 12
claps wait or sound at once; past that the quietest is let go. Played offline in 10 ms blocks, a
strike's thunder begins within 2 ms of its first sound's arrival at 1, 4 and 12 km
(`tests/wind_thunder_play_test.cpp`, and the `storm` lab's table).

## The picture

A renderer reads the flashes alive in its frame's window (`flashes_between` over
[t − dt, t]). The `storm` lab's night pictures light a side view on the CPU; the GPU frame draws
it as below.

## The storm drawn

**The lamps.** Each flash in the frame is a lamp in the frame's uniforms (`render/view.hpp`'s
`Flashes`, at most four, `flash_info` and `flashes`): the channel's middle, its reach (half its
extent), its luminous intensity over the frame and the optical depth of cloud its light crosses
on the way out. A game takes them from `brightest` (or `light_of`): the intensity is the power
times `kUnitCandela`, 3·10⁷ cd. A first return stroke of energy 1 over a 1/60 s frame then lights
a face 1 km away with about 30 lx, and one 8 km away about as a full moon does. Optical records
put a return stroke's light at 10⁸–10⁹ W for tens of microseconds; the unit sits in that band once
spread over a frame.

**`render/wgsl/flash.wgsl`** turns a lamp into light (`frame.wgsl` includes it, so every pass can
call it):

- `flash_light(p, σ)` is the light at a point inside a medium: L / (4π r²) / (1 + 0.15 τ) ·
  exp(−r / 1200 m). The channel's extent softens the near field (r² plus a quarter of the reach
  squared). Light that scatters many times through a cloud of optical depth τ leaves it as a
  diffusion does, falling as one over the depth rather than exponentially. Inside the cloud
  τ = σ·r, capped at the reach and at the lamp's own depth out. A cloud a few kilometres deep
  loses the light through its top, base and sides as it spreads, so it also falls off across the
  cloud over about the cloud's depth over π (`flash_spread`, measured from the channel's half
  reach). The clouds' march adds it, times one order of scattering over 4π, to each step's light.
  Night photographs of strikes set both: the channel is the brightest thing in the frame, and the
  cloud glows only in a band a kilometre or two across where the channel leaves its base, with
  the rest of the deck dark. An in-cloud flash lights a patch of the tower round itself (sheet
  lightning), not the whole sky.
- `flash_on(p, n)` is the light on a face. For a strike it comes from the lamp's direction. For
  a flash in the cloud it comes as much from the base overhead as from the lamp. The ground's
  materials (the snow, the default terrain) and the instances (the trees) add it to the sky's
  light.
- `flash_air(d, far)` is the clear air's glow along the eye's ray: the line integral of the light
  the air (and a fog or a fall) scatters toward the eye, in closed form. The sky adds it, and a
  deck's base 1.5 km over the eye lit from inside.

The ground is not shadowed from a flash. A strike behind a ridge lights the near slope too, which
the eye forgives in a flash this short.

**The cloud's spread march.** The cloud is marched at a quarter of the pixels, a quarter of them
each frame, with the rest reprojected. A flash lasts a frame or two, so a reprojected pixel would
carry it into the dark frames after it. A frame with a lamp, and the four after it, march the
quarter whole (`FrameHistory::flash_frame`).

**The channel** (`render/bolts.hpp`, the `bolts` pass after the fall and before the resolve and
the bloom). Each segment is a camera-facing ribbon eight drawn widths across: a Gaussian core a
drawn width wide and a soft halo round it, the light the rain and the air scatter close to the
channel. Night photographs show every channel in such a glow and never with a hard edge, so the
halo is capped at half of white on the screen and fades into the night however bright the core
is. The width is the channel's true width, or 2 px at its distance if that is wider. The
segment's light is luminous intensity per metre: the flash's candela shared along the lit
segments by glow and length. Its luminance is that over the profile, so a far channel drawn wider
is dimmer and gives the same light, held under what a half-float picture can store. The air's
extinction dims it, the depth hides it behind a ridge, and it marks its whole ribbon reactive.
A frame with a flash takes no history in the resolve at all: the last frame's dark is no history
for a frame the flash lights anew, and beside a channel the neighbourhood's box is too wide to
clip it away, which left dark pixels along the channel. The post's sharpening works on the
light squashed into 0..1 for the same reason: in linear light its lobe beside a channel pulled
the neighbours below black. The bloom spreads the core's glare. The main channel is lit by every stroke and the branches only by the
first. A flash inside the cloud shows no channel (that is sheet lightning), and neither does a
strike's channel above the cloud's base.

## The labs

`make lab LAB=storm` (`docs/labs.md`): a cell's life cycle, a multicell storm's flashes minute by
minute, one flash's flicker against 60 Hz frames, night views of a ground strike, sheet lightning
and a bolt from the blue, a filmstrip, the radar view with gust fronts and strikes, thunder's
envelope at 1, 4 and 12 km with its sound, the same strike played through the synth with how late
each began, a minute and a half of the storm heard from 5 km, and the bands table above, which fails the lab when the
in-cloud share, the strokes or the interstroke time leave their band.

`make lab LAB=storm-sky` draws the storm in Fall Line's frame: a seed's map at night under
towers, with one cell laid ahead of the view where a tower stands. It shoots the night with no
flash, a strike at its first stroke, sheet lightning and a bolt from the blue, and a filmstrip of
the strike's frames at 60 a second. The lab fails when a flash's picture is no brighter than the
same view in the dark, or when the filmstrip's brightest frame is not the one the most light fell
in.

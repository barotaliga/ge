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

## The picture

A renderer reads the flashes alive in its frame's window (`flashes_between` over
[t − dt, t]). It draws three things:

- **The cloud lit from inside.** Each flash is a light at the channel's middle, with its radius
  and power (`brightest`). Inside the cloud its light diffuses rather than travelling straight:
  L / (4π r²) / (1 + 0.15 τ), with the optical depth τ ≈ σ r capped at the radius. A thick core
  then glows softly while a thin patch shows the flash sharply, and sheet lightning is that light
  seen through the cloud. M37b puts it in the frame's uniforms (`flash_info`, `flashes`) and in
  `render/wgsl/flash.wgsl`, which the clouds' march calls.
- **The channel itself.** A bright core and a halo where it runs in clear air, a glow where it
  runs inside the cloud, lit by the frame's light.
- **The sky and the ground.** The frame's one scalar (`sky_light`) lights the sky and the ground.

The `storm` lab's night pictures light a side view the same way on the CPU.

## The labs

`make lab LAB=storm` (`docs/labs.md`): a cell's life cycle, a multicell storm's flashes minute by
minute, one flash's flicker against 60 Hz frames, night views of a ground strike, sheet lightning
and a bolt from the blue, a filmstrip, the radar view with gust fronts and strikes, thunder's
envelope at 1, 4 and 12 km with its sound, and the bands table above, which fails the lab when the
in-cloud share, the strokes or the interstroke time leave their band.

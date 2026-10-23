---
name: input
description: "Use for engine/input in gameengine2 and anything a player's hands touch: keyboards, raw mice, gamepads (rumble, trigger rumble, light, gyro, touchpad), joysticks and HOTAS, steering wheels with pedals and shifters and force feedback, touch screens; the device model and events, the input state, dead zones and curves, actions, contexts and the bindings file, rebinding by pressing, the device database (roles, learning a wheel), touch controls, players and hot-plug, feedback effects, the event log, the simulated backend, engine/play/devices (a person driving the play axes), and the input-shape, input-session and input-ffb labs; the M19 rows."
---

# Input devices

`engine/input/` (`ge_input`, namespace `ge::input`) is how a player's hands reach any game on
the engine: a skier's ramped keys, a shooter's raw mouse, a racer's wheel and pedals, a flight
stick's twist, a phone's thumbs. The design is [`docs/input.md`](../../../docs/input.md); the
rows are `docs/goals.md` M19a–M19e. Everything here is generic (actions, sources, devices),
never a skier or a gun. A game's actions and its default bindings file live in the game.

| File | What |
| --- | --- |
| `input/device.hpp` | `DeviceInfo`, kinds, families, `Feature` bits, the positional `PadButton` and `PadAxis`, hats |
| `input/event.hpp` | THE EVENT: one 32-byte POD, its type table (stated once), the builders |
| `input/state.hpp` | `Queue` (a frame of a backend), `InputState`, `update`: edges, taps, slots, pooled keyboard and mouse, fingers, last used |
| `input/names.hpp` | Every key (HID usage), button, axis, kind, family and event type as text; `pad_label` for prompts |
| `input/shape.hpp` | Dead zones, the outer edge, curves, anti-dead zone, thresholds, the key ramp |
| `input/actions.hpp` | `ActionDef`, `Source`, `Binding`, contexts, chords, `evaluate`, `make_binding` |
| `input/bindings_text.hpp` | The bindings file (`ge-bindings 1`), a source as one word |
| `input/rebind.hpp` | Capture by pressing, conflicts, rebind |
| `input/device_db.hpp` | Roles and ranges for wheels, pedals, sticks, throttles; the `ge-devices 1` text; learning a row |
| `input/touch.hpp` | Touch layouts: floating and fixed sticks, buttons, levers, drags |
| `input/players.hpp` | Players: join on press, lost and returned devices |
| `input/feedback.hpp` | Rumble, triggers, light, player lights, force-feedback effects, `force_at` |
| `input/event_log.hpp` | The event log (`ge-events 1`): record and replay a session |
| `input/sim.hpp` | The simulated backend and the force-fed wheel rim |
| `input/text.hpp` | The text formats' shared word splitter and exact numbers |
| `play/devices.hpp` | A person's actions as the play contract's axes, a press latched for one step |

## Rules

- **Nothing above a backend reads a device.** A game names actions; a binding names sources;
  only a backend (engine/platform's SDL, the simulated one, a log, Steam Input) writes events.
  Never branch on a device kind in a game to read input; add a source or a binding instead.
- **Every source of an action is read at once.** Do not add an "active device" switch. The
  last used device (`InputState::last_used`) is for choosing prompts only.
- **A tap shorter than a frame is a press.** Anything that reads edges reads `pressed`, never
  "held now and not before", or a 5 ms tap at 60 Hz is lost.
- **A press reaches one step.** Through the play contract a press is latched by the frame and
  taken by the first step (`axes_step`); a frame that grants no step keeps it.
- **No dead zone on a calibrated role.** `make_binding` gives a `role:` source 0 .. 1; a wheel's
  range is its row's job, and a dead zone on top makes its centre sticky.
- **Rows are measured or learned, never guessed.** A wheel's or a stick's row comes from a real
  device's report or from `Learn`. Do not invent axis layouts for a model from memory.
- **Deterministic above the backend.** `core::libm` for any curve or wave, no clock but the
  events' stamps, so the event log replays to the bit (`input-session` fails otherwise).
- **No allocation in a frame.** `update`, `evaluate`, `touch_update` and `players_update` work
  in fixed arrays; reserve the queue once.

## Adding a device kind, a source or an effect

1. The kind, the event type or the source kind in its header, and its name in `names.cpp`, the
   bindings file's words and the event log's line, so it round-trips (the tests hold every
   name both ways).
2. The fold in `state.cpp`, the read in `actions.cpp` (`source_value`, and `is_digital` /
   `source_tapped` for a button-like source), the capture in `rebind.cpp`.
3. A simulated driver in `sim.hpp`, so a test and the `input-session` lab can drive it.
4. The backend's case in engine/platform (M19b).

## Labs

- **`input-shape`** (picture): every dead zone, curve and key ramp charted. Run it before and
  after any change to `shape.cpp` and read the curves: a step at the inner edge is radial's
  alone; the round zones keep the angle; sloped snaps near a cardinal and still reaches 1.
- **`input-session`** (scenario): keys, a mouse, a pad, a wheel with pedals and a touch screen
  through a skier's, a shooter's and a racer's maps; the event log replayed to the bit; players;
  rebinding. The moments table is the quickest read of what a change did to the actions.
- **`input-ffb`** (scenario): effects on the simulated rim. `return.settle` and
  `return.overshoot` are the numbers to hold; `ring` shows an undamped spring on purpose.

---
name: saves
description: "Use when this game saves and loads: the engine's ge::save (the save file of tagged, versioned and checksummed chunks; the state described once by describe(io, T&) for the writer and the reader; fields added and dropped across versions with since and until), what goes in a save and what the setup rebuilds, `ge save inspect` to read a file without the game, and Run::save and Run::load with `ge save write`, `load`, `check` and `diff`."
---

# Saves on ge::save

The engine's save file is `ge::save` (link `ge::save`; headers `save/*.hpp`). A save is the
game's MUTABLE state; what the run's setup rebuilds (a map generated from a seed, indices over
it) is never in it. Every part of the file is checksummed, every number little-endian and
fixed-width, so a save written on one platform loads bit for bit on every other.

## Describe each type once

```cpp
#include "save/file.hpp"
namespace sv = ge::save;

template <class Io>
void describe(Io& io, Player& p) {   // beside Player, in its namespace
  io(p.name);                        // numbers, enums, bools, strings, vectors, arrays,
  io(p.pos);                         //   optionals, pairs and other described types
  io(p.inventory);
  io.since(2, p.gold, 0);            // added in this chunk's version 2: older saves read 0
}
```

The same function writes and reads, so the two cannot drift. A struct whose memory is exactly
its described fields may be marked `GE_SAVE_PLAIN(T)` (at global scope) to copy vectors of it in
one memcpy; test it with `sv::plain_problem<T>()`, which must be empty.

## Write and read a file

```cpp
std::vector<uint8_t> bytes;           // keep it: its capacity is reused next save
sv::Meta meta{.game = "my-game", .game_version = 1, .title = "Chapter 2"};
sv::FileWriter w(bytes, meta);
w.put(sv::tag("PLYR"), 0, 2, player);            // tag, instance, version, value
for (uint32_t i = 0; i < enemies.size(); i++) w.put(sv::tag("ENMY"), i, 1, enemies[i]);
w.finish();

sv::FileReader r;
if (!r.open(bytes, "slot-1", "my-game")) show(r.problem());   // damaged, newer, another game
r.get(sv::tag("PLYR"), 0, 2, player);  // 2: the newest version this build reads
```

`sv::peek` reads the header and the meta from a file's first `sv::kPeekBytes`, which is how a
list of saves is shown without opening each. `ge save inspect <file>` prints a file's chunks or
the one line saying why it is refused.

## The run on the play contract

Give the game's `ge::play::Run` a `save` and a `load` (engine/play/game.hpp): `save` puts the
run's chunks into the `FileWriter` it is handed, `load` reads them back into a run the save's
setup has just opened. The engine writes the meta (the game, the setup as the input log's
header, the time played) and its own `PLAY` chunk (the step), so a save reopens on its own:

```cpp
bool save(sv::FileWriter& f) const override { f.put(sv::tag("WRLD"), 0, 1, world_); return true; }
bool load(sv::FileReader& f, std::string& error) override {
  if (!f.get(sv::tag("WRLD"), 0, 1, world_)) { error = f.problem(); return false; }
  return true;
}
```

Describe the WHOLE mutable state, the bot's and the pilot's included; leave out only what the
setup rebuilds (the map, indices, pointers to them) and what is not the run's (a debug hook).

```sh
ge save write runs/my-game-38.inputs --step=7200 --out=saves/a.save   # a log played and saved
ge save load saves/a.save --steps=1200      # loaded and played on by the pilot; the digests
ge save check runs/my-game-38.inputs --every=10   # THE CHECK: every save steps on as the run never saved
ge save diff saves/a.save saves/b.save      # the chunks that differ, then the state's fields
```

## The check every game should have

`ge save check` saves a log's run every few seconds, loads each save into a run already played
on (so a field left out keeps a wrong value instead of a lucky default), plays it to the log's
end beside the run never saved and names the first step any digest parts; a run that reaches
the end must also save to the same bytes. Hold it in a test (`ge::play::check_saves`) on the
game's longest runs. A field a step writes but the describe leaves out shows up at the first
step that reads it; `ge save diff` of the two runs' saves names the field.

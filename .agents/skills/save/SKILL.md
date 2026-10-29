---
name: save
description: "Use for anything about SAVES in gameengine2: engine/save (the save file's header, chunks, directory and XXH64 checksums; the state described once by describe(io, T&) for the Writer and the Reader; since and until; GE_SAVE_PLAIN), a game's or a plugin's state made savable, the save-file lab, `ge save inspect`, and the M25 rows (the run saved and loaded on the play contract, slots and the store, pictures, characters, cloud saves, the content cache)."
---

# Saves

`docs/saves.md` is the design and the plan; the rows are M25a–M25h in `docs/goals.md`. This skill
is the procedure. Read its lessons first: `make lessons ARGS=save`.

## The rules

- **A save is the state, never the inputs.** It holds what a step changes; what the setup
  rebuilds (the map, the indices over it) is shared and never saved. The meta's `setup` says
  what reopens it.
- **Describe a type once.** One `template <class Io> void describe(Io& io, T& v)` beside the
  type, in its namespace (argument-dependent lookup finds it). Never a separate save and load.
  A field a step writes and the describe leaves out is the classic save bug: the run saved and
  loaded parts from the run never saved at the first step that reads it.
- **Change a describe with a version.** Bump the chunk's version, read a new field with
  `io.since(v, field, fallback)`, an old one with `io.until(v, scratch)`; a bigger change
  branches on `io.version()`. Never change what an existing version reads.
- **Bulk where it is honest.** Numbers, enums and their vectors and arrays are one memcpy. A
  struct may be `GE_SAVE_PLAIN(T)` only when `plain_problem<T>()` is empty (no padding, every
  byte a described field, in order); put that check in the type's test.
- **No pointers into the setup.** Refer by index; the shared, immutable parts (the map, its
  indices, `GameState::level`) are rebuilt by the setup and left out of the describe. A
  `shared_ptr` a step can replace (a bot's plan's track) is saved as its own copy; a debug hook
  (`ai::Why*`) is not the run's and is left out.
- **Refusals are prompts.** A reader never throws and never reads past its bytes: it keeps the
  first problem with its byte, and `FileReader` names the file, the chunk and the next action.

## Making a type savable

1. Write its `describe` beside it (a plugin's type in its plugin, a game's in the game).
2. Pick its chunk: a four-letter `tag("XXXX")`, an instance (one per racer, per machine), a
   version starting at 1.
3. Test it: a value written and read back writes the same bytes (`save_io_test.cpp` is the
   pattern), and a plain type's `plain_problem` is empty.
4. For a run: `Run::save` puts the chunks, `Run::load` gets them (both walk the same fields; a
   group of private members goes through one private template, as Fall Line's `pilot_fields`),
   and `check_saves` (`ge save check`, the `save-run` lab, `tests/play_saves_test.cpp`) holds
   every save to the run never saved.

## Checking a run

```sh
make lab LAB=save-run                                  # every Fall Line scene, a save every 10 s
build/debug/ge save check runs/fall_line-38.inputs     # a log's saves against the run never saved
build/debug/ge save diff a.save b.save                 # which chunk and which field of `state`
```

A save that parts: `ge save write` the log at the step it parted from and at the step before,
`ge save diff` them, and look for the field a step wrote that no describe lists. A new member in
a described struct is the usual cause: add it to the describe (and bump the chunk's version
with `since` when saves already exist).

## Checking a file

```sh
make lab LAB=save-file                      # round trip, every cut and flip refused, versions, speeds
make lab LAB=save-file ARGS="--racers=256"  # a bigger state; time it on PRESET=release
build/debug/ge save inspect <file>          # the header, the meta and every chunk, or why it is refused
```

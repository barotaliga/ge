# Sound: recorded sounds in space, ambience, rooms, music, voice

How a classic game is heard: its recorded sounds played where they happen, its ambience by the
place the player stands, the room's reverb, the soundtrack, the menu's clicks, and players' voices.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/audio/pcm.hpp`, `sample_voice.hpp`, `resample.hpp` | A recorded sound (WAV, its loop cue), its voice at any rate, offline resampling |
| Engine | `engine/audio/channels.hpp` | Sounds in a world: distance gain and pan, entity channels, the channel limit and stealing |
| Engine | `engine/audio/ambient.hpp`, `rack.hpp` | Ambient beds: looped layers whose levels walk to targets (`Loop::whole`) |
| Engine | `engine/audio/room.hpp`, `reverb.hpp` | The room round the listener probed by rays (mean free path, open share, RT60), the reverb it makes |
| Engine | `engine/audio/music.hpp`, `music_console.hpp`, `flac.hpp` | The soundtrack: a stereo track off the game thread, `cd` and `bgmvolume` |
| Engine | `engine/net/voice.hpp`, `voice_codec.hpp`, `engine/console/talk.hpp` | Voice chat: Opus, the gate and push-to-talk, the host's relay by route, the jitter buffer, the mix; the talk commands |
| Engine | `engine/audio/meter.hpp` | A sound checked by its meters when nobody can listen |
| Classic enhance | `plugins/classicenhance/sound.hpp` | The pack's sounds denoised, at 48 kHz, the top band rebuilt (`ce_sounds`) |
| Shell | `games/shooters_edge/sound/ambience.hpp` | Ambience by leaf: water and sky levels off the map |
| Shell | `games/shooters_edge/client/listen_sound.hpp`, `demo_sound.hpp`, `demo_heard.hpp`, `menu_sound.hpp`, `own_sounds.hpp` | The window's own game heard, a recording's sounds, the menu's clicks from the game's recordings, the game's own WAVs under the classic names |
| Shell | `games/shooters_edge/net/qw_talk.hpp`, `client/voice_out.hpp`, `client/demo_voice.hpp` | Who hears a chat line or a voice on our server (all, team, near by earshot and walls, a slot), the voice message, voices in recordings |
| Shell | `games/shooters_edge/profiles/remix_sounds.hpp` | Remix's sound pairs by role |
| Classic | `plugins/classic/q2_footsteps.hpp` | Footsteps by surface: a texture's `.mat` material, its step set, the surface under the stepper, never the same step twice running |
| Classic | `plugins/classic/q2_loop_merge.hpp` | Copies of one looped sound merged into one voice, each ear held to full: Quake II's entity loops (`al_merge_looping`), Quake's level loops (always) |
| Shell | `games/shooters_edge/client/room_ear.hpp` (`s_mixahead`) | How far ahead the device mixes, one setting for every classic game (Quake's spelling `_snd_mixahead`) |
| Classic | `plugins/classic/content/soundtrack.hpp` (`music_file`) | A track by its file's name under `music/` (`ogg play <name>`, a server's music by text) |

## Each game's

| | Quake | Quake II | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Files | WAV under `sound/` | WAV under `sound/`; player sounds by figure | WAV and other formats | sound lumps in the archive (a new reader) |
| Ambience | by the map's leaves | by the map's entities' loop sounds | entity loops | sector sounds |
| Footsteps | none (remix's `cr_sounds q2` makes Quake II's: `g_footsteps 1` on the classic rules) | its own, by the surface's `.mat` material (`cl_footsteps`) | its own | none |
| Looped sounds of one kind | the level's loops of one file merged into one, always | entity loops merged into one (`al_merge_looping`) | merged | none |
| The music switched off | stops (`cd off`) | pauses where it is (`ogg_enable`, the console's `off_pauses`) | | |
| Voice on our server | `*ge_ext` voice bit, `sv_voip` off by default | the same route, its own message id | the same route | the same route |

## Adding a game

1. `open_sound` in the adapter; every sound by the game's own names. Loops: a file looped whole
   needs `Loop::whole` (`Loop::file` loops only with a cue point).
2. Its ambience and entity loop sounds through the shared beds.
3. Its voice and chat on our server through `qw_talk.hpp`'s route: the game supplies only its
   protocol ids and what a TEAM is (Quake's `team` user info key; Quake II's skin or
   model by its deathmatch flags). The voice server setting is off by default where the game never
   had voice.
4. Classic enhance's sound stage on its packs, counted against them.
5. Remix's sound pairs in `remix_sounds.hpp`, by role then strength.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Voice chat and `say_near` by earshot and walls | ours, offered to both games | `engine/net/voice.hpp`, `qw_talk.hpp` |
| Entity loop sounds | Quake II | the shared beds |
| The soundtrack kept as FLAC, played with no disc | ours | `engine/audio/music.hpp` |
| Sounds denoised and band-extended | ours | `ce_sounds` |
| A material's own footsteps, the music by name | Quake II | `plugins/classic`, `engine/audio/music_console.hpp` |
| The OpenAL path's extras on the one mixer: device choice, two ears, merged loops | Quake II | `engine/platform`, `engine/audio`, `plugins/classic` |

| Drawback left behind | How |
| --- | --- |
| A disc in the drive for music | ripped once to `data/<id>/music/` |
| Every room sounding the same | room acoustics (`ce_acoustics`) |
| Telephone-band 11 kHz recordings | the rebuilt top band, by choice |
| Two sound back-ends (software and OpenAL), a restart to switch | one mixer that does what both do; `s_enable` 2 is 1 |

## Gaps

- Room acoustics are in the engine and switched by `ce_acoustics`; the wiring into the window
  is in progress.
- The sector family's sound lumps and its music format.

## Labs and tests

`se-audio`, `se-ambient`, `se-talk`, `synth`, `audition`; `tests/audio_*_test.cpp`.

## Lessons

- Tune acoustics on real maps: bare-stone absorption gave 6 to 12 s tails; 0.22 gives 1 to 5 s.
  Measure RT60 at every spawn with a chart lab, not by ear.
- A game's server that passes only part of the user info to its rules breaks skin-based teams
  silently: check what reaches the rules layer.
- A game uses only its own sounds outside remix.
- A weapon's own sounds stay its own under remix: a weapon swapped in is its game's whole, so
  `cr_sounds` never renames a shot, a blast, a bounce or a hit (`SoundMatch::weapon`).
- Two games ship many sounds by one name (`player/gasp1.wav`, `items/protect.wav`): under remix the
  played game's copy hides the other's on the search path, so "heard as the other game" must read
  that game's own copy below it (`DemoSounds::hear_as`'s `theirs`), not the name's winner.
- Test a sound part through the hearing the window runs (`ListenHearing`, `DemoHearing`), not the
  table alone: the table was right while a joined game never heard it
  (`tests/shooters_edge_remix_heard_test.cpp`).

---
title: Check the window plays what remote_step gathers; the live recording keeps voice, not own voice
date: 2026-11-07
scope: games/shooters_edge/net/classic_remote.cpp, games/shooters_edge/client/main.cpp, games/shooters_edge/net/classic_record.cpp
concepts: [remote-client, demo-heard, voice, live-recording, push-to-talk, microphone]
---

`remote_step` gathers every frame's sounds, voices, prints and centre message into
`remote.demo.state`, but the window's `DemoHearing::frame` takes a `DemoHeard`, and it was handed
the played demo's (`bench.heard`), which is empty on a joined server: a joined game was silent
and its chat never printed, with every test green. When a remote path feeds a shared player, test
what the window hands it, not only what the state holds. For voice, the remote's recorder is
`live` (voice and named effects kept, no voice header) but not `voice`, so `record_own_voice`
splices nothing and the player never hears himself; the microphone is `ClientConfig::live_mic`
fed through `remote_mic`, and talking follows the `+voip` button read at each `remote_step`.

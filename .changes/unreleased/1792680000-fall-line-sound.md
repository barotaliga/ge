---
type: Added
title: The skier's beds and Fall Line's sound
---

`plugins/skier` gains a presentation half, `ge_skier_app` (published as `ge::skier_app`): the wind on the skier's body (rush, roar, whistle, flutter, buffet) and his skis on the snow (the groomer's hiss, the powder's hush, the edge, chatter and the skid) as steered beds, read off a `SkierState` by `ride_bed_update`. A plugin's `app/` half is now built when its `ge_add_plugin` call names an `APP` list. Fall Line gains its bank of one-shots, the event route (gates, misses, the finish, landings, wipeouts, trunks, shoulders, resets), the listener table and the faders, the `audition` lab, and a `fall_line` window that plays the bot's run through the audio device.

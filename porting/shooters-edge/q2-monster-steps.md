# Answers: Quake II monsters' footstep frames, by model frame

Source read: Yamagi Quake II (yquake2), the release cloned for this clean room, its game library.

How the absolute frame numbers were derived (applies to every number below): each monster's game code names every model frame of its model with a 0-based number, in the model's frame order; an animation is given as a first and a last model frame plus one entry per frame. A step frame's absolute number is the animation's first model frame plus the step's offset within the animation (offset counted from 0). All absolute numbers below are [derived] this way. I did not open the .md2 files themselves, so that the game's numbering matches the .md2's frame index is [derived] from the numbering being a contiguous 0-based list per model, not [measured].

General facts used by every answer [read]:
- A monster's animation advances one model frame per 0.1 s (10 Hz). At each tick the frame to show is chosen, then the entry of the current animation at offset (shown frame minus the animation's first frame) is run; a step is the action attached to that entry.
- When the current animation ends (the last frame was shown), its end action runs (often choosing the next animation); if the shown frame then lies outside the current animation's range, the first frame of that animation is shown, so offset 0 of a new animation does run (a step at offset 0 sounds on entry).
- If the shown frame already lies inside a newly chosen animation's range, the frame simply advances by one from where it is; there is no reset to the first frame.

## 1. Tank and tank commander

- Same code for both kinds (the commander only changes skin, health and similar) [read]. Sound "tank/step.wav", registered at spawn [read].
- Volume 1, normal attenuation, body channel [read].
- No condition: g_monsterfootsteps is not consulted, no ground check, no speed check [read]. It sounds whenever the animation reaches the step entry, in the air, in pain or in an attack [read].
- Stepping animations, absolute model frames [derived]:

| Animation | Model frames | Step frames |
| --- | --- | --- |
| start walk | 30 to 33 | 33 |
| walk (loop) | 34 to 49 | 41, 49 |
| stop walk | 50 to 54 | 54 |
| start run | 30 to 33 | 33 |
| run (loop) | 34 to 49 | 41, 49 |
| stop run | 50 to 54 | 54 |
| pain 3 (the heaviest pain) | 206 to 221 | 221 |
| blaster attack, the closing part | 71 to 76 | 76 |
| strike attack | 77 to 114 | 83, 87, 114 |
| rocket attack, the opening part | 115 to 135 | 130 |
| rocket attack, the closing part | 145 to 167 | 160 |

- Walk and run use the same model frames and the same step frames (start, loop and stop each pair) [derived].
- Walk loop rate: two steps per 16 frames, i.e. 0.8 s apart (frames 41 and 49 alternate at 0.8 s per cycle, 1.6 s per loop) [derived].

## 2. Mutant

- Sounds "mutant/step1.wav", "mutant/step2.wav", "mutant/step3.wav", one picked at random each step from the game's random number generator, about equally likely [read; "equally" derived from picking by remainder of division by 3].
- Volume 1, normal attenuation, on the VOICE channel, not the body channel [read]. So a step shares the channel with the mutant's sight, idle, pain and attack cries (the engine replaces a sound playing on the same channel of the same entity) [derived].
- No condition: no switch, no ground or speed check [read]. Registered at spawn [read].
- Stepping animations [derived]:

| Animation | Model frames | Step frames |
| --- | --- | --- |
| start walk | 126 to 129 | 128 |
| walk (loop) | 130 to 141 | 130, 132, 136 |
| run (loop) | 56 to 61 | 57, 59 |

- No pain, death or attack animation of the mutant steps [read].

## 3. Jorg (the first final boss)

- "boss3/step1.wav" is the left step and "boss3/step2.wav" the right step; each frame names which one (not random) [read]. These two sounds are registered only by Jorg's spawn and played only by Jorg; the Makron and the stand-alone boss figure (the ending's static Makron, a different entity) do not use them [read].
- Volume 1, normal attenuation, body channel [read]. No switch, ground or speed check [read].
- Stepping animations [derived] (L = boss3/step1.wav, R = boss3/step2.wav):

| Animation | Model frames | Step frames |
| --- | --- | --- |
| stand (idle loop, 51 frames) | 112 to 162 | 146 L, 150 R, 159 L, 162 R |
| run (loop) | 168 to 181 | 168 L, 175 R |
| pain 3 | 87 to 111 | 89 L, 91 R, 107 L, 111 R |

- Jorg's walk uses the same model frames as his run (168 to 181) but has NO steps [read]. See question 6.
- Pain 3 plays only when one hit does more than 100 damage, with a 30% chance, and never on the hardest skill (which has no pain animations) [read].
- Stand loop: four steps every 5.1 s [derived].

## 4. Makron

- "makron/step1.wav" is the left step, "makron/step2.wav" the right step, chosen by frame, not random [read]. Volume 1, normal attenuation, body channel [read]. No switch, ground or speed check [read].
- Stepping animations [derived] (L = makron/step1.wav, R = makron/step2.wav):

| Animation | Model frames | Step frames |
| --- | --- | --- |
| walk (loop) | 477 to 486 | 477 L, 481 R |
| run (loop) | 477 to 486 | 477 L, 481 R |
| death 2 (95 frames; the torso is thrown off as it starts) | 251 to 345 | 254 L, 268 R, 286 L, 306 R, 308 L, 315 R, 317 L, 320 R, 323 L |

- Walk and run are the same frames and the same steps [derived]. The death animation is the Makron's normal (non-gib) death [read].

## 5. The kinds behind g_monsterfootsteps

All of these sound only when g_monsterfootsteps is non-zero (already in our note). Frame numbers [derived]; "same frames" notes [derived].

**Infantry (enforcer)**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| fidget | 1 to 49 | 6, 48 |
| walk | 74 to 85 | 74, 80 |
| run | 92 to 99 | 93, 97 |
| pain 1 | 100 to 109 | 104, 109 |
| pain 2 | 110 to 119 | 114, 119 |
| duck | 120 to 124 | 124 |
| death 1 | 125 to 144 | 129, 133 |
| death 2 | 145 to 169 | 150, 151 |
| firing attack (attack 2) | 199 to 206 | 202 |

No two stepping animations share frames.

**Soldier (light, normal and SS share one model and one code)**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| attack 4 | 39 to 44 | 40, 44 |
| pain 3 | 62 to 79 | 64, 79 |
| run | 99 to 104 | 100, 103 |
| running attack (attack 6) | 109 to 122 | 111, 115, 118, 121 |
| walk 1 | 215 to 247 | 218, 223 |
| walk 2 | 256 to 265 | 256, 260 |

No two stepping animations share frames.

**Gladiator**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| walk | 7 to 22 | 11, 19 |
| run | 23 to 28 | 25, 28 |

**Iron maiden**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| rocket attack start | 0 to 12 | 8 |
| rocket attack (loop) | 13 to 26 | 15, 17, 22, 25 |
| rocket attack end | 27 to 31 | 31 |
| slash start | 32 to 34 | 34 |
| slash end | 44 to 47 | 47 |
| death 2 | 60 to 82 | 63, 70, 76 |
| walk | 191 to 200 | 192, 197 |
| run | 191 to 200 | 192, 197 |

Walk and run share frames 191 to 200 with the same steps.

**Berserker**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| walk | 25 to 35 | 28, 33 |
| run | 36 to 41 | 37, 40 |
| club attack | 84 to 95 | 86 |
| strike attack | 96 to 109 | 98, 104, 109 |

Correction to our note: the walk spans 11 model frames (25 to 35), not 12. The game lists 12 entries for it, but the animation's range stops at 35, so the twelfth entry is never reached [read/derived]. Its steps (offsets 3 and 8) are unaffected.

**Medic**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| walk | 0 to 11 | 1, 7 |
| run | 102 to 107 | 102, 106 |
| pain 2 | 116 to 130 | 119, 130 |
| cable attack (reviving a corpse) | 209 to 236 | 216, 230 |

Addition to our note: the cable attack (offsets 7 and 21 of 28 frames) also steps [read]. It plays when the medic attacks while in its healing mode, i.e. while it is going to revive a dead monster [read].

**Gunner**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| walk | 76 to 88 | 76, 81, 88 |
| run | 94 to 101 | 95, 99 |
| run and shoot | 102 to 107 | 103, 106 |
| chaingun attack start | 137 to 143 | 138 |
| chaingun attack end | 152 to 158 | 158 |
| pain 1 | 159 to 176 | 161, 171, 175 |
| pain 2 | 177 to 184 | 179, 184 |

**Brain**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| walk | 0 to 10 | 3, 9 |
| run | 0 to 10 | 3, 9 |
| attack 1 | 53 to 70 | 56, 70 |
| pain 1 | 88 to 108 | 90, 107 |
| duck | 146 to 153 | 149, 153 |

Walk and run share frames 0 to 10 with the same steps.

**Parasite**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| run (loop) | 70 to 76 | 72, 73, 75 |
| walk (loop) | 70 to 76 | 72, 74, 75 |
| stop running | 77 to 82 | 79 |
| stop walking | 77 to 82 | 79 |

Run and walk share frames 70 to 76 but step at different frames (73 in the run, 74 in the walk). The two stop animations share 77 to 82 with the same step. The parasite's step plays its tap sound ("parasite/paridle1.wav"), volume 1, idle attenuation, body channel [read].

**Insane marine**

| Animation | Model frames | Step frames |
| --- | --- | --- |
| normal walk | 160 to 172 | 166, 172 |
| normal run | 160 to 172 | 166, 172 |
| crazed walk | 173 to 198 | 179, 184, 191, 197 |
| crazed run | 173 to 198 | 179, 184, 191, 197 |
| standing pain | 199 to 209 | 203 |

Normal walk and normal run share frames and steps; so do crazed walk and crazed run. (Our note lists only the walks; the runs are the same frames with the same steps [read].)

## 6. Is a step tied to the animation, not to the model frame?

Yes [read]. The step belongs to an entry of a particular animation; at each tick the game looks up the entry by the shown frame's offset inside the CURRENT animation. The same model frame shown by another animation runs that other animation's entry, which may have no step.

Where it matters (the same model frames used by two animations with different steps) [derived from comparing every animation of each kind]:
- Jorg: frames 168 to 181 step at 168 and 175 in the run, but never in the walk. Answer: yes, tied to the animation.
- Parasite: in frames 70 to 76, frame 73 steps only in the run and frame 74 only in the walk; 72 and 75 step in both. Answer: yes.

Where it does not matter, because the animations sharing frames have identical steps: tank (start, loop and stop of walk and run), Makron (walk and run), iron maiden (walk and run), brain (walk and run), insane marine (walk and run, normal and crazed), parasite's two stop animations. Every other stepping animation listed above uses model frames no other animation of its kind uses together with a step [derived].

Edge cases [read/derived]:
- Switching between two animations that share frames (for example the parasite from walk to run) does not restart the cycle: the frame keeps advancing from where it is, and from that tick the new animation's steps apply.
- A held frame (the duck "hold" some kinds use) re-runs only that frame's own action each tick; no step frame is a held frame, since each entry carries a single action [derived].
- A step on an animation's last frame sounds when that frame is shown, before the end action picks what comes next [read].
- Death animations keep stepping while they play (infantry, iron maiden, Makron); nothing checks for death or ground before a step [read].

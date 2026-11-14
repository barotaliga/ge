# The sequel's light, brightness and sky as its bundled client draws them

From a clean-room reader (the `clean-room` skill) over the sequel's bundled community client
(Q2PRO, the git snapshot of 2025-12-11), its OpenGL renderer, manual and menu, 2026-11-14. Each
fact is marked **read** (the source or the manual states it) or **derived** (worked out from what
the source does). Nothing was measured: a lab that checks a fact marks it **measured** here.
The sky's keys, files and orientation are in `sky-box.md`, its fog in `fog-alpha.md`; this note
adds what they leave open.

"The default path" means the client's defaults: the GLSL backend on (`gl_shaders 1`), cube-map
skies off (`gl_cubemaps 0`), software gamma (`vid_hwgamma 0`). Where the older fixed-function path
(`gl_shaders 0`) differs, it is said. Values are on a luxel's or texel's 0 to 255 scale unless they
say otherwise. The numbered headings follow the questions the reader answered.

**The one fact that matters most** (derived): at the defaults a wall shows texel × luxel / 255 ×
2, clipped per channel at 255, and the sky box shows its texel × 1. A renderer that multiplies
texel by luxel / 255 with no factor 2 draws the sequel's maps at half brightness.

## The lightmaps and the textures' brightness

**1. A face's colour.**

- The default path, per pixel (read): the texel (0 to 1) times the lightmap sample (0 to 1, from
  an 8-bit texture, bilinear) plus `gl_brightness`, times `gl_modulate`, times
  `gl_modulate_world`, times `intensity`. In words: texel × (luxel/255 + brightness) × modulate ×
  modulate_world × intensity. The only clip is the framebuffer's, per channel at 1.0 (derived:
  the shader clamps nothing, so a channel past 1 saturates on its own and the hue shifts toward
  white).
- The settings (read):
- `intensity`: default **2**; clamped to 1..5 when the renderer starts (an out-of-range value is
  written back as the bound). The menu offers 1x, 2x and 3x ("texture intensity").
- `gl_modulate`: default 1, archived; clamped to 0 and above (upper bound 1,000,000). It also
  scales the entities' light.
- `gl_modulate_world`: default 1, same clamp; the world only.
- `gl_brightness`: default 0, clamped to -1..1, added to the lightmap in 0..1 units on the
  default path; the menu's "lightmap brightness" slider runs 0 to 0.3.
- So at the defaults a wall shows texel × luxel/255 × 2 (derived). A luxel of 128 shows the
  texel at about its own value; a luxel of 255 shows it doubled, clipping per channel at 255.
  A renderer that multiplies texel by luxel/255 with no factor 2 is exactly half as bright.
- Changes to modulate, brightness and intensity take effect at once on the default path; no
  lightmap is rebuilt (read).
- The older path (read): the textures are multiplied by `intensity` when loaded, each channel
  capped at 255 (min of texel × intensity and 255); brightness (× 255) is added to and modulate
  multiplied into the luxels when the lightmap is built, before the clamp of question 2; the GPU
  then multiplies texture by lightmap with no further factor. Changing any of these needs the
  lightmaps or textures rebuilt. The manual says the GLSL path was made to avoid this path's
  "washed out" colours.

**2. When the luxels are built (and when they are loaded).**

- Loading the map does not touch the luxels (read): they are kept as stored.
- A face's lightmap is built at map load and again whenever one of its styles changes value or
  a dynamic light touches it (read). The light is the style sum plus the dynamic lights
  (questions 6 and 9); on the older path only, brightness and modulate are applied to it here.
  Negative channels become 0. If the largest channel is above 255, all three are scaled by 255
  over that largest channel, so the clamp **keeps the hue** rather than clipping each channel.
  The lightmap saturation blend (question 3) follows, and each channel is truncated to a byte.
- The lightmap pages are 256 × 256 luxels for a stock map (1024 for maps with decoupled
  lightmaps), set by `gl_lightmap_bits` (default 0 = automatic, range 7..10) (read).

**3. Lightmap colour.** `gl_coloredlightmaps`, default **1**, clamped 0..1, the menu's "lightmap
saturation" (read). Each luxel is blended toward its luminance (0.2126 R + 0.7152 G + 0.0722 B)
by that amount: luminance + (channel − luminance) × setting. 1 keeps full colour, 0 is grey,
there is no boost past 1 (read). It applies after the hue-keeping clamp, and to the entities'
light too (read). At the defaults the maps' coloured light is carried unchanged.

**4. The textures' own brightening.**

- On the default path no texel is scaled at load (read): `intensity` is applied in the shader
  (question 1), only to the faces question 7 names.
- `gl_saturation`, default 1, clamped 0..1, the menu's "texture saturation": the same luminance
  blend applied to wall textures at load, not to warping (liquid) ones, not to skins or the sky
  (read). Needs the textures reloaded.
- `gl_invert`, default 0: inverts wall textures at load (read).

**5. Gamma.** `vid_gamma`, default **1**, archived; the menu's "texture gamma" slider runs from
1.3 down to 0.3 (read). No clamp in the renderer (read).

- The curve (read): out = 255 × ((in + 0.5) / 255.5) raised to the power gamma, plus 0.5,
  capped at 255 and truncated. Gamma below 1 brightens.
- Software gamma, the default (read): the curve is applied at load to wall textures and model
  skins only (and to 2D pictures if `gl_gamma_scale_pics`, default 0, is on). The lightmaps, the
  entities' light, the sky and the particles are not gamma-corrected. A change needs the textures
  reloaded. At gamma 1 nothing is done.
- On the older path the wall and skin texels get gamma first, then the intensity scale (read).
- Hardware gamma (`vid_hwgamma 1`, default 0, needs a video restart): the same curve is loaded
  into the display's gamma ramp, so it covers the whole screen and changes at once; the textures
  are then not touched (read).
- There is no gamma or tone step in the shader (read).

**6. Light styles.**

- A face has up to four styles; its light is the sum over them of that style's luxel map times
  the style's current value (read). The sum then goes through question 2's clamp.
- A style string's letter becomes (letter − 'a') / 12, so a = 0, m = 1.0, z = 25/12 ≈ 2.083
  (read). An empty string means 1.0 (read).
- The time step (read): one letter per 100 ms (10 letters a second), the index being the
  client's game clock in milliseconds divided by 100, modulo the string's length. With
  `cl_lerp_lightstyles 1` (default 0) the value is blended linearly between the letter and the
  next by the fraction of the 100 ms; a one-letter string is constant.
- `gl_dynamic`, default 1 (read): 0 makes every style read style 0's value (no flicker) and
  turns dynamic lights off; 2 makes styles 1 to 31 read style 0's value but leaves styles 32 and
  up (the switchable lights) working.
- 256 styles (read).

**7. Surface flags.**

- Light flag (0x1): no special drawing (read). Such faces are lit by their lightmap like any
  wall and get the intensity factor (derived); their glow is whatever the light compiler put in
  their luxels. (The flag matters only to a cube-map-sky option that hides fake sky faces.)
- Sky flag (0x4): no lightmap; the face only marks where the sky box shows (question 11) (read).
- Warp flag (0x8), translucent 0x10 and 0x20, on stock maps: **no lightmap** (read). They draw
  their texture times a vertex colour (read):
- 0x10: alpha 85/255 (one third), RGB unscaled,
- 0x20: alpha 170/255 (two thirds),
- warp, opaque: alpha 1,
- the intensity factor is **not** applied to them, so they show the texel at its own value,
  with one exception: an opaque warping face whose texture name contains "lava" gets the
  intensity factor (×2) (read).
- On the older path the same vertex colour carries 255/intensity in RGB to undo the texture's
  load-time intensity (read).
- On maps carrying the extended BSPX lumps (the re-release's), with the GLSL path, the warping
  and translucent faces keep their lightmaps, and every face with a lightmap gets the intensity
  factor (read). Stock maps' liquid light offsets are said to be broken, which is why (read).
- Translucent faces are not hit by the entities' light probe (question 8) (read).

## Entities' light

**8. A model's light.**

- Where it comes from (read): a line straight down 8192 units from the entity's origin, against
  the world and every brush model in view; the first face hit that has a lightmap (sky, warp and
  translucent faces skipped) gives the sample. The sample is bilinear over the four luxels round
  the hit point, each style's map times its value, summed. If nothing is hit, or the map has no
  light data, the light is white (1, 1, 1). Maps with a light grid (BSPX) use the grid instead,
  trilinear over eight cells (`gl_lightgrid`, default 1).
- The scale (read): the sample gets `gl_brightness` × 255 added, is multiplied by `gl_modulate` ×
  `gl_modulate_entities` (default 1, clamp 0 and above), negatives go to 0, the hue-keeping clamp
  at 255, the lightmap saturation blend, then it is divided by 255. Its colour is kept.
- Then (read): dynamic lights are added (question 9); then, with `gl_doublelight_entities`
  (default **1**), the result is multiplied by `gl_modulate` × `gl_modulate_entities` once more.
  `r_fullbright` (a cheat, default 0) makes it white.
- The minimum light (read): only for entities flagged minimum-light (the client sets it on the
  first-person weapon; the game may set it too). If the colour's length (the square root of the
  sum of squares) is 0 it becomes (0.1, 0.1, 0.1); if it is below 0.1 it is scaled up to length
  0.1. Other entities have no minimum.
- The glow flag (bonus items) (read): each channel plus 0.1 × sin(7 × time in seconds), never
  below 80% of its value.
- Each channel then clipped to 0..1 (read). Full-bright entities get 1, 1, 1; shells get their
  fixed colour.
- Shading by direction, `gl_dotshading` default **1** (read): each vertex's colour is the light
  times (1 + d), where d is the dot product of the vertex normal with a fixed unit direction, and
  a negative d is first multiplied by 0.3. So the factor runs from 0.7 (facing away) to 2.0
  (facing it). The direction is 45 degrees above the horizontal; its horizontal part is fixed in
  the world along the map's +x axis whatever the model's yaw (derived from the model-space
  direction being turned by minus the yaw). With dot shading off the factor is 1.
- Finally the shader multiplies the skin texel by that colour and by `intensity` (×2 at the
  default) (read). So a model under a luxel of 128 with its normal toward the light shows its
  skin at about 0.5 × 2 × 2 = 2 times its texel before the framebuffer clip (derived).

**9. Dynamic lights.**

- Up to 64 a frame (read). A light has an origin, an intensity (its radius in world units) and
  an RGB colour, each channel nominally 0..1; a negative colour darkens (read).
- On faces (read): a face is reached when its plane is within (intensity − 64) of the light. For
  that face, rad = intensity minus the light's distance from the plane; under 64 it adds nothing.
  The light's point projected on the plane is compared with each luxel's centre, the in-plane
  distance taken as the larger of the two axis offsets plus half the smaller (in world units).
- `gl_dlight_falloff` 1 (the default): luxels nearer than rad − 51.2 get rad − distance ×
  rad / (rad − 51.2), falling linearly from rad at the centre to 0 at the edge.
- 0: luxels nearer than rad − 64 get rad − distance, falling from rad to 64 and then cut off
  (the classic hard rim).
- The amount, times the colour, is added in luxel units to the style sum **before** question 2's
  clamp and before the shader's modulate and intensity, so a dynamic light is scaled exactly
  like a static one (×2 at the default).
- On entities (read): (intensity − 64 − distance from the entity's origin) / 255 times the colour,
  when positive, added after the world sample's scaling but before the second modulate.
- Common lights at the default settings (read): a rocket in flight 200, (1, 1, 0); blaster and
  hyperblaster bolts 200, (1, 1, 0); a muzzle flash 200 plus 0 to 31 (100 plus 0 to 31 when
  silenced), mostly (1, 1, 0), held for `cl_muzzlelight_time` (default 16 ms); a rocket or grenade
  explosion 350, (1, 0.5, 0.5), its intensity fading with the explosion's alpha, (16 − frame) / 16
  per 10 Hz frame. A flag in `cl_dlight_hacks` (default 0) makes the rocket's light orange
  (1, 0.23, 0) or explosions smaller (200).

## The sky

**10. The turn.**

- The angle (read): with the classic protocol the box always turns: angle = `skyrotate` degrees a
  second times the client's game clock in seconds. With the extended protocol the server sends
  "rotate autorotate"; autorotate 1 turns it as above, autorotate 0 holds it at a fixed angle of
  `skyrotate` degrees.
- The clock (derived): the client's game clock, which follows the server's frames (milliseconds,
  reset to 0 when the client connects to a level, frozen while the game is paused). Not the
  wall-clock time and not the light styles' blend.
- The direction (read): the axis is normalised first; if its length is below 0.001, or the rate
  is 0, there is no turn. The box is turned by the standard right-hand rotation about the axis:
  looking down the axis from its tip toward the origin, the box turns counter-clockwise as the
  angle grows. Directions from the eye are turned back by the same rotation to find the face
  texel.

**11. The sky's brightness.**

- The box's pictures are drawn with their texel colour as is: no lightmap, no `intensity`, no
  vertex colour, and on software gamma not gamma-corrected; no saturation (read).
- So the sky shows at the brightness of a lit wall of the same texel under a luxel of 127.5:
  half of what a wall under a full luxel of 255 shows (derived).
- Fog, when a map or the server sets it, mixes the sky toward the fog colour by the fog's sky
  share (read; the share is in fog-alpha.md). Hardware gamma, when on, covers it with the rest of
  the screen (read).
- The pictures are filtered bilinearly (`gl_bilerp_skies`, default 1) and their texture
  coordinates kept 1/512 inside each edge to hide seams (read).
- The box's corners are drawn round the eye at the world's size: 2048 units, or 4096 if the
  world's largest extent is above 2048, or 8192 above 4096 (read). Only the six faces' parts
  that the map's sky faces cover are drawn (read).

**12. A missing sky.**

- The client has no default sky name; the name comes only from the server (read). An empty name
  looks for pictures named only by their suffix, which do not exist (derived).
- In this snapshot, if **any** of the six `.tga` pictures is missing, or the name is too long, the
  whole box is dropped: every sky face draws black and there is no turn (read). This differs from
  the earlier note (one missing face drawn as the "no texture" pattern), which came from other
  clients or older versions.
- `gl_drawsky 0` also draws the sky black (read).
- With `gl_cubemaps 1` (default 0) the client first looks for a single cube-map picture
  `sky/<name>.tga` and falls back to the six faces; maps whose sky faces name an `env/sky`
  texture then get a two-layer scrolling sky, the first game's kind (read). At the defaults there
  is no cloud layer.
- The screen behind is not cleared each frame by default (`gl_clear 0`, `gl_clearcolor` black)
  (read).

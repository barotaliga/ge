# Answers: an entity's look fields (scale, colormod, alpha) in QC and on the wire

Read: FTE (FTEQW) trunk at commit f09ac7c, dated 2026-11-02. DarkPlaces trunk at commit 6d754e4,
dated 2026-11-05, was opened only for its published description of the three extensions (the QC
extension definitions it ships), used where FTE says nothing.

Tags: [read] = stated by the source. [derived] = worked out from what the source does.
[measured] = run here. Nothing in this document was measured.

Terms used below. "Entity delta" means the QuakeWorld packet-entities delta with FTE's extension
bits. "Player update" means the QuakeWorld player-info message. "Replacement deltas" means FTE's
newer entity protocol, which current FTE clients negotiate in place of the other two; it is
mentioned only where its bytes match.

## 1. DP_ENT_SCALE

- Field: `.float scale`. The engine registers it itself, so it exists on every entity even when the
  mod does not declare it. [read]
- 0 means 1 (normal size). FTE's own field description and DarkPlaces' both say so. [read]
- Range on a server (entity delta and replacement deltas): the value is multiplied by 16 and
  clamped to 1..255, then stored as a byte with the fraction dropped. So the smallest scale is 1/16
  (0.0625), the largest is 255/16 (15.9375), and the step is 1/16. A negative scale becomes 1/16.
  [read, derived]
- Around the origin: yes. FTE scales the model's three axes about the entity's origin. Only the
  Hexen II game mode can move that pivot (to the model's centre, bottom or top), and FTE sets the
  pivot to the origin for every other game. [read]
- It applies to any model the entity draws, because it scales the whole model matrix. [derived]
- Collision box: unchanged. Nothing in FTE's movement, trace or link code reads scale. The only
  physics code that reads it is the optional rigid-body physics plugin. Mins and maxs stay what
  setsize gave them. [read, derived]
- Player update: players' scale uses a different encoding, given in answer 4. [read]

## 2. DP_ENT_COLORMOD

- Field: `.vector colormod`. It is engine-registered like scale. [read]
- '0 0 0' means "no tint", drawn exactly like '1 1 1'. [read]
- Range: each component is multiplied by 32, truncated to an integer and clamped to 0..255, so a
  component can be 0 to 255/32 (7.96875), in steps of 1/32. Negative values become 0. A vector with
  any non-zero component is sent as it is. '0 0 0.01' therefore makes the model black, because it
  is not exactly '0 0 0'. [read, derived]
- DarkPlaces' description gives the same meaning and says the range goes up to about '8 8 8'.
  [read]
- Fullbrights: in FTE the tint does NOT apply to fullbright (glowing) texels. FTE's own field
  description says so, and its default model shader multiplies the lit colour by the tint first,
  then adds the fullbright texels scaled by a separate field, `.vector glowmod`. glowmod has the same
  '0 0 0'-means-'1 1 1' rule and the same 1/32 byte encoding. [read]
- Dynamic lights: in FTE's default model shader the tint multiplies the model's whole lighting
  result. That lighting already includes the dynamic lights folded into the model's light sample,
  so the tint is applied after them. FTE also has real-time per-pixel light passes; the reader did
  not settle whether those passes apply the tint. [read, open]
- The tint's fourth channel, used as one colour, is the entity's alpha (answer 3). [read]

## 3. DP_ENT_ALPHA

- Field: `.float alpha`. It is engine-registered. [read]
- 0 means 1 (fully opaque), for compatibility. FTE's and DarkPlaces' descriptions agree. [read]
- Range on a server (entity delta and replacement deltas): 0 becomes byte 255 ("not set"). Any other
  value is multiplied by 254, clamped to 1..254, and the fraction is dropped. So 1.0 and above
  becomes 254 (opaque), and a negative alpha becomes 1, which is nearly invisible (1/254).
  DarkPlaces' description tells mods to use -1 for invisible; in FTE that is only nearly invisible.
  [read, derived]
- Ghost dimensions: when the viewer shares only a "ghost" dimension with the entity (the
  QSG_DIMENSION_PLANES fields dimension_see, dimension_seen and dimension_ghost), the byte is
  multiplied by `.float dimension_ghost_alpha`, or by 0.5 when that field is 0. This happens after
  the clamp, so 255 can become 127, and a byte of 1 can become 0. [read, derived]
- What else it changes, client side: an entity drawn below full opacity is marked translucent. It
  is sorted with blended surfaces, it casts no shadow from FTE's real-time lights, and particles do
  not collide with it. [read]
- It does not make the entity additive. Additive drawing is a separate effects flag. [read]

## 4. On the wire

### Entity delta: the extension bits [read]
- After the usual 16-bit header word and the "more bits" byte, bit 7 of the "more bits" byte says
  an "even more" byte follows. Bit 7 of that byte says a "yet more" byte follows.
- Scale is bit 0 of the "even more" byte. Transparency is bit 1. Colourmod is bit 2 of the "yet
  more" byte (bit 10 when the two extension bytes are counted as one 16-bit word).
- Each one is sent only to a client that negotiated that extension: the FTE extension flags
  "scale" (0x00000002), "trans" (0x00000008) and "colourmod" (0x00080000). A field is sent only
  when its byte differs from the previous state or baseline.

### Entity delta: order of the payload [read]
1. The model index (one byte, or two bytes with the larger model-index extension).
2. Frame, colormap, skin, effects (each one byte).
3. Origin x, angle x, origin y, angle y, origin z, angle z, in that interleaved order, each one
   when its bit is set.
4. Scale (1 byte).
5. Transparency (1 byte).
6. Fatness (1 signed byte).
7. Hexen II draw flags (1 byte), then absolute light (1 byte).
8. Colourmod: red, green, blue (3 bytes).
9. The rest: DP render flags (1 byte), tag entity and tag index, light (four 16-bit values, then
   style and flags bytes), then the effects high byte.

### Entity delta: the bytes [read, derived]
- Scale byte b: the client decodes it as b / 16. The server encodes truncate(clamp(scale × 16, 1,
  255)), and sends 16 when the field is 0. A server never sends 0. A client that receives 0 draws
  the model unscaled, because FTE's renderer treats a scale of 0 like 1. The default (no bit, or
  the baseline) is 16, meaning 1.0.
- Colourmod bytes: each is decoded as b / 32 (written in the source as b × 8 / 256, the same
  number). Each is encoded as clamp(truncate(component × 32), 0, 255). '0 0 0' is sent as 32 32 32.
  The default is 32 32 32. A client treats exactly 32 32 32 as "no tint", and any other triple as a
  forced tint.
- Transparency byte b: below 254, the entity is drawn translucent with alpha b / 254. 254 and 255
  are both opaque. 255 is the default and what the server sends for alpha 0 ("not set"). 254 is
  what it sends for an explicit alpha of 1 or more. A server sends 0 only through the
  ghost-dimension multiplication above. Received 0 means alpha 0, so the entity is invisible but
  still marked translucent. The byte's meaning was not followed for brush-model water.
- The same three encodings (scale × 16, colourmod × 32, alpha × 254 with 255 for "not set") are
  written in replacement deltas and in the static-entity message.

### Player update [read, derived]
- After the 16-bit flags word comes one extra flags byte, but only to a client that negotiated the
  hull-size, trans, scale or fatness extension, and only when bit 15 of the word says it follows.
  In that byte, scale is bit 16, transparency bit 17, fatness bit 18 and colourmod bit 19, counting
  the word and the byte as one flags value.
- Order: after the weapon frame come scale (1 byte), transparency (1 byte), fatness (1 signed
  byte), then the hull number (1 byte). The client then reads a colourmod triple (3 bytes) when
  bit 19 is set.
- Scale in a player update uses a DIFFERENT encoding. The server writes the field times 50,
  truncated to an integer, and only the low byte goes on the wire. The client decodes b / 50. The
  byte is sent only when scale is neither 0 nor 1, so the range is 0.02 to 5.1. A larger value
  wraps: 6.0 gives 300, whose low byte is 44, decoded as 0.88. When the byte is absent the scale
  is 1.
- Transparency in a player update also uses a DIFFERENT encoding. The byte is sent only when alpha
  is not 0, and it is alpha × 255 converted straight to a byte. The client decodes b / 255 and
  marks the player translucent for any value other than 255. So 255 is opaque, alpha 1.0 sends 255,
  and a tiny alpha (below 1/255) sends 0, an invisible player. An alpha above 1 or below 0
  overflows the byte conversion, which C leaves undefined. When the byte is absent the alpha is
  255.
- The ghost-dimension multiplication applies to the player's alpha before the × 255.
- Colourmod for players: the client can read it, but FTE's server never sets bit 19 in this
  message. A player's colormod therefore never travels in the player update, and players draw with
  the client's default 32 32 32 (no tint). Players' colormod reaches clients only through
  replacement deltas, where players are ordinary entities.
- Scale and transparency are not sent in a player's update to that player himself, unless he
  negotiated split-screen.

## 5. Extension names and map keys

- checkextension names a server advertises: "DP_ENT_SCALE", "DP_ENT_ALPHA" and "DP_ENT_COLORMOD".
  [read]
- Called with just the name, each is reported present, because the server supports every protocol
  extension. Called with a second argument, a client entity, each is reported present only when
  that client negotiated the matching wire extension (scale, trans or colourmod) or negotiated
  replacement deltas. [read]
- A player can hide any one of them with the setting pr_ext_<lower-case name> (for example
  pr_ext_dp_ent_alpha), which defaults to 1. [read]
- The related "FTE_PEXT_SPAWNSTATIC" means static entities can carry alpha, scale and the rest.
  [read]
- Map entity text: the map's key/value pairs are matched to entity fields by name. Because the
  engine registers scale, alpha and colormod as fields itself, a map's "scale", "alpha" and
  "colormod" keys (colormod written as three numbers separated by spaces) set them on any entity
  the mod spawns. [derived]
- Static entities: makestatic captures the entity with the same encoder as a moving one, so its
  scale, alpha and colormod are kept. A client with the static-entity extension, or with
  replacement deltas, receives a static entity as a delta from the default state, carrying any of
  the three bytes that differ from the defaults. A client without either receives the plain
  static-entity message, which has none of them. [read]

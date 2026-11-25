# Quake II: areas and area portals — answers

Source read: the stock tree's qcommon (cmodel, qfiles), server (sv_world, sv_ents, sv_send,
sv_game, sv_ccmds, sv_init), game (g_ai, g_func, g_misc, g_utils, g_spawn, g_save, p_weapon),
client (cl_ents, cl_view, client.h, ref.h) and the two renderers' leaf walks (gl_rsurf, r_bsp).
The ctf game folder carries the same area code as game. Each fact is marked [read] (seen
directly) or [derived] (worked out from what was read). No code is reproduced.

## 1. The map file's area data

- Leaf record (version 38 map, leafs lump): 28 bytes, little-endian, in this order: contents
  (signed 32-bit, offset 0), cluster (signed 16-bit, offset 4), area (signed 16-bit, offset 6),
  mins (three signed 16-bit, offset 8), maxs (three signed 16-bit, offset 14), first leaf face
  (unsigned 16-bit, offset 20), leaf face count (unsigned 16-bit, offset 22), first leaf brush
  (unsigned 16-bit, offset 24), leaf brush count (unsigned 16-bit, offset 26). [read field order
  and types; offsets and total derived]
- The loader reads the area field as a signed 16-bit value and keeps it per leaf; CM_LeafArea
  returns it (error "bad number" for an out-of-range leaf). [read]
- Areas lump is lump index 17 (LUMP_AREAS); area portals lump is index 18 (LUMP_AREAPORTALS). [read]
- Area record: 8 bytes, two signed 32-bit fields in this order: the number of area-portal records
  belonging to this area, then the index of its first area-portal record. [read] A lump size not a
  multiple of 8 is an error ("funny lump size"); more than MAX_MAP_AREAS (256) areas is an error
  "Map has too many areas". [read] The loaded copy adds two runtime ints per area, a flood number
  and a flood-valid stamp, both zeroed at load. [read]
- Area portal record: 8 bytes, two signed 32-bit fields in this order: the portal number (the
  index into the open/closed table), then the other area (the area on the far side). [read]
  Each area lists the portal records that lead out of it, so a portal between areas A and B is
  expected to appear once in A's list (other area B) and once in B's (other area A), both with the
  same portal number. [derived: the flood only walks outward from each area's own list; the map
  compiler is not in this tree, so the two-records-per-portal layout is not read here]
- MAX_MAP_AREAPORTALS is 1024 and the open-state table has 1024 entries, but the portals-lump
  loader checks the record count against MAX_MAP_AREAS (256) instead and reports "Map has too many
  areas" if the count exceeds 256. [read] So a map with more than 256 portal records fails to load
  in the stock code. [derived]
- Area 0 is special: it is "not used". The flood loop starts at area 1 and never seeds area 0;
  area 0 keeps flood number 0 (from load), while every flooded area gets a flood number of 1 or
  more. [read] Leaves in area 0 are typically solid/outside leaves. [derived] The areas lump does
  contain a record for area 0 (the area count includes it). [derived: the flood loop runs to the
  count starting at 1, and area indices go up to count minus 1]
- With no map loaded (cinematic server, empty name) the area count is forced to 1. [read]
- func_areaportal's portal number comes from its "style" key, parsed as an integer into the
  entity's style field (the field's comment says it is also used as the areaportal number). [read]
  The brush of a func_areaportal carries the CONTENTS_AREAPORTAL bit (0x8000). [read: the bit
  exists; that the compiler uses it to split areas is derived]
- func_areaportal is never given a model, solid or link by the game: its spawn function only sets
  its use function and its count to 0 ("always start closed"). [read]

## 2. Connectivity: flood, state, query

- State per portal: a plain boolean flag (portalopen), 1024 entries, indexed by portal number. Not
  a count. [read] All flags are cleared to closed at map load, and also when the server reloads
  the same map without flushing (the cached-map path), but not on a client-side load of the map.
  [read]
- Setting a state (CM_SetAreaPortalState, the game's SetAreaPortalState import): checks the portal
  number is not greater than the record count (error "areaportal > numareaportals"; note the test
  is strictly greater, so a number equal to the record count passes, and negatives are not
  checked), stores the flag, then re-floods the whole map immediately. So every set call refloods,
  even if the value did not change. [read]
- Two doors sharing one portal: there is no reference count; the last call wins. If two doors
  target the same func_areaportal and one finishes closing while the other is open, the portal is
  closed. [derived from the flag]
- Flood (FloodAreaConnections): increments a global flood-valid stamp, resets a running flood
  number to 0, then for each area from 1 to count minus 1 in index order: skip it if already
  stamped this pass, else increment the flood number and flood from it. [read]
- Flood from an area (recursive): if the area already carries this pass's stamp, return if it has
  the same flood number, else raise a fatal-drop error "FloodArea_r: reflooded"; otherwise set its
  flood number and stamp, then for each of its portal records in order, if that record's portal
  number is open, recurse into the record's other area with the same flood number. [read] So areas
  get flood numbers 1, 2, 3... in order of the lowest-index area of each connected group. [derived]
- Connected query (CM_AreasConnected, the game's AreasConnected import), in order: (1) if the
  cvar map_noareas is non-zero, answer connected; (2) if either area is greater than the area
  count, fatal-drop error "area > numareas" (strictly greater; equal passes); (3) connected exactly
  when the two areas' flood numbers are equal. [read]
- Consequences for area 0: area 0's flood number stays 0 and every real area's is at least 1, so
  area 0 is connected only to itself; area 0 with any area 1 or above is "not connected". [derived]
  A map whose areas lump is empty (count 0) answers connected for area 0 with area 0. [derived]
- The cvar is map_noareas, default "0", no flags, registered at each map load. It only affects the
  query and the area bits (below); the flood still runs. [read]
- Area bits (CM_WriteAreaBits): byte count is (area count + 7) / 8, rounded down. If map_noareas
  is set, every byte is 255 ("for debugging, send everything"). Otherwise the buffer is zeroed and,
  for every area i from 0 to count minus 1, bit i (byte i/8, bit i mod 8, least significant first)
  is set when area i's flood number equals the given area's, or when the given area is 0. [read]
  So a viewer in area 0 gets every bit set; a viewer in area 1 or above never gets area 0's bit.
  [derived] Maximum 32 bytes (256 areas); client and server frames hold 32 bytes. [read sizes:
  MAX_MAP_AREAS/8]
- Saves: the server's level file (.sv2) holds the configstrings followed by the raw portal-open
  table (1024 entries of the boolean enum, so 4096 bytes on the usual 4-byte enum); reading it back
  restores the table and refloods. [read table write/read and reflood; 4096 bytes derived] The
  level file is read when re-entering a level in single player (not in deathmatch). [read]

## 3. An entity's area(s) when linked (SV_LinkEdict)

- Every link (except the world entity and entities not in use) resets both areas to 0 and the
  cluster count to 0, then gathers all leaves touched by the absolute box, solids included, up to
  128 (MAX_TOTAL_ENT_LEAFS). [read]
- The absolute box: origin plus mins/maxs, or for a SOLID_BSP model with any non-zero angle, the
  origin plus or minus the largest absolute component of mins and maxs on every axis; then
  expanded by 1 unit on all six sides. [read]
- Areas are taken before the solid-type check, so SOLID_NOT entities (for example the player
  noise markers) get areas too. [read]
- Rule, walking the gathered leaves in the order the tree walk returned them: a leaf with area 0
  is ignored. For a non-zero area: if the first area is already set and differs from this one,
  this one becomes the second area (overwriting any earlier second area; while the server is
  loading, a debug print "Object touching 3 areas at ..." is made when a third distinct area
  replaces a different second one); otherwise this becomes (or stays) the first area. [read]
  So: first area = the first non-zero area seen; second area = the LAST non-zero area seen that
  differs from the first. Doors "may legally straddle two areas, but nothing should ever need more
  than that". [read comment; first/last characterisation derived]
- Cap: if the leaf gather hit 128, the entity falls back to its top node for visibility; areas are
  still only those of the first 128 leaves gathered. [read fallback; area consequence derived]
- The entity has no area of its own beyond these two numbers (areanum, areanum2). [read]

## 4. The stock server's uses of areas

Entities sent to a client (SV_BuildClientFrame), per frame:
- The viewer's point is the player-state origin (stored in eighths, so times 0.125) plus the view
  offset. Its leaf gives the client area and the client cluster. [read]
- The frame's area bits are written from the client area (section 2) before entities are chosen.
  [read]
- For each entity from 1 upward, tests in this order: (1) skip if SVF_NOCLIENT; (2) skip if it has
  no model index, no effects, no looping sound and no event; (3) if the entity is the client's own,
  skip all further tests and send it; otherwise (4) area test: if the client area is not connected
  to the entity's first area, then unless the entity has a non-zero second area that IS connected,
  skip ("blocked by a door"); (5) beams (RF_BEAM) test only their first cluster against the
  client's PHS; everything else tests its clusters (or the top node in fallback) against the fat
  PVS; (6) an entity with no model index (sound/effect only) is skipped if more than 400 units from
  the viewer point. [read]
- No other exception to the area test exists (the world is never in this loop; the client's own
  entity is the only bypass). [read] An entity whose first area is 0 (touches only area-0 leaves)
  is connected only for a viewer in area 0, and a viewer in area 0 receives only area-0 entities.
  [derived]
- Looping sounds (an entity's sound field) travel with the entity state and so pass the same area
  test, then the fat PVS (the code picks the fat PVS for both sounding and silent entities; the PHS
  alternative is commented as intended but not used), then the 400-unit cull if the entity has no
  model. [read]

Sounds started by the game (SV_StartSound; both the entity sound and the positioned sound
imports land here):
- If no origin is given, a SOLID_BSP entity's origin is its origin plus the middle of its box,
  otherwise its origin. [read]
- The PHS is used unless the channel has bit 8 (CHAN_NO_PHS_ADD, then the channel is masked to its
  low 3 bits) or the attenuation is exactly ATTN_NONE (0). With PHS the message is multicast PHS
  (reliable form if channel bit 16, CHAN_RELIABLE); without, multicast ALL (reliable form likewise).
  [read] So attenuation none and the no-PHS bit skip both the PHS and the area test. [derived]

Multicast (SV_Multicast), used for sounds and temp entities alike:
- For the PVS and PHS kinds, the origin's leaf gives the source area and its cluster's PVS or PHS
  row is the mask. For ALL, there is no mask and no test. [read]
- For each client slot: skip free or zombie; skip if not spawned unless reliable. Then, only when
  there is a mask, the client's leaf is found from the client entity's origin (the entity origin,
  NOT the view point used for entity selection), and: first the area test (source area vs client
  area, skip if not connected), then the cluster bit in the mask (skip if clear). [read]
- So temp entities and positioned/entity sounds multicast with PVS or PHS (reliable or not) are
  both cluster-filtered and area-filtered; ALL is neither. [derived]

Game imports (sv_game):
- inPVS(p1, p2): leaf of p1 gives cluster, area and the PVS row; leaf of p2 gives cluster and area.
  False if the row exists and p2's cluster bit is clear; then false if the areas are not connected;
  else true. [read]
- inPHS(p1, p2): the same with the PHS row ("doors block hearing"). [read]
- SetAreaPortalState and AreasConnected are the collision functions passed straight through. [read]

## 5. The stock game's uses

Monster hearing (FindTarget in g_ai):
- A noise is chosen in this order: (a) if a monster was sighted-and-angered within the last frame
  (sight entity frame >= current frame minus 1) and the monster is not an ambusher (spawnflag 1),
  that is used as a sight target, not a heard one; (b) else if the player's own noise (self or
  weapon kind) was made within the last frame, it is heard; (c) else if the monster has no enemy,
  is not an ambusher and an impact noise was made within the last frame, it is heard; (d) else the
  current sight client is checked by sight. [read]
- Then: the target must be in use; if it is already the enemy, return found; a heard noise (not a
  client, not a monster) whose owner has FL_NOTARGET is rejected. [read]
- Heard branch, in this exact order: (1) ambusher (spawnflag 1): require visible() (a line trace
  eye to eye against MASK_OPAQUE: solid, slime, lava); otherwise require inPHS from the monster's
  origin to the noise's origin, which itself tests the PHS cluster bit first and then area
  connectivity of the two points' leaves; (2) reject if the straight distance from monster origin
  to noise origin is greater than 1000 units; (3) if the noise entity's first area differs from
  the monster's first area, reject when AreasConnected says not connected (second areas are not
  consulted); (4) turn to face it, mark it a sound target and take it as enemy. [read]
- Noises are not made at all in deathmatch, nor by a FL_NOTARGET player; a silenced weapon shot
  uses up a silencer shot instead. [read] The two noise markers per player are SOLID_NOT,
  SVF_NOCLIENT, a 16-unit box, linked at the noise point each time, so their areas are current.
  [read]

Monster sight:
- No area test anywhere in the sight branch: range not FAR, light level above 5, visible() (the
  MASK_OPAQUE trace), then a facing test depending on range. A closed door blocks sight only by
  being a solid brush model in the trace. [read; door-as-blocker derived]

func_areaportal:
- Starts closed (count 0); the game does not set the portal at spawn (the collision code already
  starts all closed). [read]
- Its use toggles its own count (XOR 1) and sets the portal state to the new count. [read] It does
  not read the current portal state, so its count can disagree with a door that also drives the
  same portal. [derived]
- Generic target firing (G_UseTargets) skips a func_areaportal target only when the user is a
  func_door or func_door_rotating ("doors fire area portals in a specific way"); any other
  user (button, trigger, relay...) toggles it. [read]

Doors (func_door and func_door_rotating share these functions):
- The door opens its portals by finding every entity whose targetname equals the door's target
  and whose classname is func_areaportal (case-insensitive), and setting that portal number open or
  closed directly (no toggle, no count). A door without a target does nothing. [read]
- Opening: at the START of moving up (door_go_up), after the move is set up and after G_UseTargets
  runs, the portals are set open. If the door is already moving up, nothing happens; if it is at
  the top, only its wait is renewed (no portal call). [read]
- Closing: the portals are set closed only when the door has FULLY closed (door_hit_bottom, after
  the bottom state is set). Starting down (door_go_down) does not touch them. [read]
- A blocked door reversing from down to up calls door_go_up, so the portal is set open again
  (it was still open). [read; "still open" derived]
- Each member of a door team runs its own go-up / hit-bottom, so each member drives the portals
  its own target names. [read]
- Start open (spawnflag 1): the door's start and end positions are swapped and it is in the bottom
  state at the open position. Only a door that gets the auto trigger (no health and no targetname,
  team leader) opens its portals at spawn, in the trigger-spawning think. A start-open door with a
  targetname or health leaves its portals closed at start. [read] After that, since "up" now moves
  to the closed position and "hit bottom" arrives at the open position, such a door sets its
  portals open while closing and closed when it reaches open. [derived; this is the stock
  behaviour, not a reading error]
- Killed doors (health) call the use path, so they open like a used door. [read]

func_door_secret:
- Used (or killed): if it is at its rest origin, it starts its first move and sets its portals
  open immediately (same lookup as doors). It does not call G_UseTargets. [read]
- It sets its portals closed in its final step, after the last move back to rest finishes. If its
  wait is -1 it stays open and never closes the portal. [read]

Others:
- No other game code touches the portal state or area connectivity: plats, trains, rotating,
  buttons (except through a func_areaportal target they fire generically), func_explosive, water
  and triggers do not. [read by search of the game folder]

## 6. The client

- The frame message carries, after the frame numbers and the suppress count, one byte of area-bit
  length followed by that many bytes. The client reads them into the frame's 32-byte area bits.
  [read]
- The view's refdef points at the current frame's area bits each rendered frame; the menu's player
  view passes none. [read]
- Renderer (both GL and software): while walking world leaves already in the PVS, a leaf whose
  area bit is clear is skipped, so its surfaces are not marked for drawing ("check for door
  connected areas"). With no area bits pointer, nothing is skipped. [read] The software renderer
  also skips clipping a brush model's face fragment into a world leaf whose area bit is clear.
  [read]
- Entities are not culled by area on the client; the server already culled them. Client sounds do
  no area test. [read by search: no other use of the area bits in client code]

## Not answerable from this tree

- How the map compiler assigns area numbers and writes the portal records (two per portal, the
  order, the style key's use by the compiler): the compiler is not in this source. Stated above as
  derived where it matters.

# QuakeC bytecode, specified

The compiled QuakeC a mod ships (`progs.dat`, `qwprogs.dat`), in our words (README.md, the rules).
Little endian throughout. Sources as in `formats.md`; the behaviour below was written by a
research session that read the modern multiplayer server's source (the M18o session), so the
building session takes it from here and does not look further. Where the server's behaviour is
undefined or unsafe (an unchecked index, an out-of-range cast), this note says so and names what
our VM does instead under **Ours**.

## The file

Header, fifteen little-endian i32 (60 bytes) in this order:

| # | Field | Rule |
| --- | --- | --- |
| 0 | version | must be 6, else refuse ("wrong version: N, expected 6") |
| 1 | crc of the engine-side layout | 54730 for the multiplayer layout below; anything else is refused (the single-player layout is a different field and global order and is not in M18c) |
| 2–3 | statements: offset, count | statement 0 is never executed (an "error" slot) |
| 4–5 | global definitions: offset, count | |
| 6–7 | field definitions: offset, count | |
| 8–9 | functions: offset, count | function 0 is the null function |
| 10–11 | strings: offset, size in bytes | the first byte is 0: string offset 0 is the empty string |
| 12–13 | globals: offset, count in 32-bit words | the initial values of every global |
| 14 | entity size in 32-bit words (`entityfields`) | |

| Record | Size | Fields |
| --- | --- | --- |
| statement | 8 | opcode (u16), a, b, c (**signed** 16-bit: global offsets, or a relative jump) |
| definition | 8 | type (u16; bit 15 = saved with a game, never set on a field definition: refuse a file where it is), offset (u16, in words), name (i32 string offset) |
| function | 36 | first statement (i32; negative = builtin number, its absolute value), first parameter's global (i32), locals (i32: words of parameters plus locals, starting at the first parameter's global), profile (i32, a runtime counter: ignore the file's value), name (i32), file (i32), parameter count (i32), parameter sizes (8 × u8: 1 or 3 words each) |

**Types** (low 15 bits of a definition's type): 0 void, 1 string, 2 float, 3 vector, 4 entity,
5 field, 6 function, 7 pointer. Every value is one 32-bit word, a vector three. Floats are
IEEE single precision; arithmetic is done in single precision (no wider intermediate except
where this note says so).

**Ours, the checker** (the server checks almost nothing; a game that loads a player's file
must): every section lies inside the file; counts are not negative; every statement's opcode
is known; every operand that names a global is inside the globals (allowing three words for a
vector operand); every jump lands inside the statements; every function's first statement is
inside the statements (or is a builtin); its parameter and local range is inside the globals;
its parameter count is 0..8 and each size is 1 or 3; every name is a string offset inside the
strings table; every field definition's offset (plus 3 for a vector) is inside the entity size.
A refusal names the section, the record's index, the field and the bad value, and says what to
do (rebuild the mod, or report it).

## Memory

- **Globals**: the file's global block, copied at load (a reload starts from the file's values
  again). Offsets 0..27 are reserved: 0 is the null slot, 1–3 the return value, then eight
  parameter slots of three words at 4, 7, 10, 13, 16, 19, 22, 25. 28 onward are the engine-side
  globals in the fixed order below, then the program's own.
- **Entities**: an array of up to 2048 entities (the protocol's limit), each `entityfields`
  words, all zero on clearing. Entity 0 is the world; entities 1..32 are the client slots.
- **An entity value** (in a global, a field, a parameter) is the entity's number times its size
  in **bytes** (`entityfields × 4`); world is 0. Mods compare and pass these values; they never
  do arithmetic on them that a game can see, so the encoding only has to round-trip, but the
  world must be 0 and two values must be equal exactly when they name one entity.
- **A field value** is the field's offset in words inside an entity.
- **A pointer** (made only by ADDRESS) is a byte offset from the start of the entity array:
  `entity value + field × 4`.
- **A function value** is an index into the functions; 0 is null.
- **A string value** is 0 (the empty string), a positive offset into the file's string table,
  or a handle the engine made (the server uses negative numbers into two tables of 1024 entries
  each: strings the engine holds for the map's lifetime, and strings a builtin creates).
  **Ours**: any encoding that keeps 0 as empty and the file's offsets as they are; an engine
  string never collides with a file offset. A **temporary** string (a builtin's result built on
  the fly) lives in a ring of 8 buffers of 2048 bytes: it is valid until 8 more temporaries are
  made. Mods copy one they keep with a builtin, so the ring's depth of 8 is part of the contract.

## Calling a function

The engine (or a CALL) runs function *f* this way:

1. **Enter**: push the caller's statement index and function on the call stack (at most 32
   deep: deeper is the error "stack overflow"). Save the `locals` words that start at *f*'s
   first-parameter global onto a local stack (2048 words in all: more is "locals stack
   overflow"). Then copy each parameter from its slot (parameter *i* from global 4 + 3*i*, as
   many words as its size) into *f*'s parameter globals, packed one after another from the
   first-parameter global. Start at *f*'s first statement.
2. **Run** statements one at a time (below).
3. **Leave** (on RETURN or DONE): restore the saved words from the local stack into *f*'s range,
   pop the call stack and continue after the caller's CALL. When the stack is back at the depth
   the engine's call started from, the engine's call returns.

Recursion works because a function's locals are saved on entry and restored on leave; a global
a function writes outside its own range is shared.

An engine call to function 0 or past the last function is an error ("NULL function"). Before
calling a hook the engine sets `self`, `other` and `time` as the caller's rules say (the game
loop's note, `gameloop.md`).

**The run-away guard**: each engine call (each top-level entry into the VM, including one a
builtin makes while QC is running) may run 100000 statements; the 100000th is the error
"runaway loop". **The profile**: every statement run adds one to the running function's
counter; `profile` lists the ten highest and clears them.

**Errors** stop the program: the server prints the faulting statement, the call stack (each
frame's file and function name, innermost first), the message, and drops the map. **Ours**: the
same facts in the engine's diagnostics shape (function, statement index, file, and the line if
the file carries a line table), and the game decides what a fault ends.

## The opcodes

`a`, `b`, `c` are the statement's operands; `A` means the global (or three) at `a`. Float truth
is "not equal to zero" (so NaN is true and -0 is false) unless a row says it tests the raw word.
Results of comparisons and logic are the floats 1 or 0.

| Op | Name | Does |
| --- | --- | --- |
| 0 | DONE | as RETURN with `a` (in practice 0): copy three words from `a` to the return slot and leave |
| 1 | MUL_F | C = A × B |
| 2 | MUL_V | C = A·B, the dot product, summed left to right in float: (A0B0 + A1B1) + A2B2 |
| 3 | MUL_FV | C (vector) = A (float) × each of B |
| 4 | MUL_VF | C (vector) = each of A × B (float) |
| 5 | DIV_F | C = A ÷ B (IEEE: a division by zero gives an infinity or NaN, never an error) |
| 6 | ADD_F | C = A + B |
| 7 | ADD_V | C = A + B per component |
| 8 | SUB_F | C = A − B |
| 9 | SUB_V | C = A − B per component |
| 10 | EQ_F | C = A == B (IEEE: NaN equals nothing; -0 equals 0) |
| 11 | EQ_V | all three components equal |
| 12 | EQ_S | the two strings' **contents** are equal (the empty string and string 0 are equal) |
| 13 | EQ_E | the two words are equal as integers |
| 14 | EQ_FNC | the two function indices are equal |
| 15 | NE_F | C = A != B |
| 16 | NE_V | any component differs |
| 17 | NE_S | the contents differ. The server stores the raw result of its byte comparison, so the value is some non-zero number when they differ. **Ours**: 1 when they differ, 0 when not (mods only test it for truth) |
| 18 | NE_E | integer words differ |
| 19 | NE_FNC | function indices differ |
| 20–23 | LE, GE, LT, GT | C = A ≤ B, A ≥ B, A < B, A > B (IEEE: false with a NaN) |
| 24 | LOAD_F | C = entity A's field B (one word). A field value B below 0 gives 0 (the server guards this one family against a negative field, which a client's command can produce) |
| 25 | LOAD_V | C = entity A's vector field B (three words); no negative guard in the server: **Ours** refuses a bad field as an error |
| 26–29 | LOAD_S, LOAD_ENT, LOAD_FLD, LOAD_FNC | as LOAD_F (one word, the same negative guard) |
| 30 | ADDRESS | C = a pointer to entity A's field B. Writing the world: if A is the world **while the map is running** (after the spawn functions), error "assignment to world entity"; during spawning it is allowed |
| 31, 33–36 | STORE_F, STORE_S, STORE_ENT, STORE_FLD, STORE_FNC | B = A (one word, copied as bits) |
| 32 | STORE_V | B = A (three words) |
| 37, 39–42 | STOREP_F, STOREP_S, STOREP_ENT, STOREP_FLD, STOREP_FNC | the word at pointer B = A |
| 38 | STOREP_V | the three words at pointer B = A |
| 43 | RETURN | copy three words from `a` to the return slot (1–3) whatever the function's type, and leave |
| 44 | NOT_F | C = (A == 0) as a float test (so NOT of -0 is 1) |
| 45 | NOT_V | all three components == 0 |
| 46 | NOT_S | A is string 0 or the empty string |
| 47 | NOT_ENT | A is the world |
| 48 | NOT_FNC | A is function 0 |
| 49 | IF | if the **raw word** at A is not zero, jump by `b` statements (relative to this one; so -0.0, whose bits are not zero, is true here although NOT_F calls it false) |
| 50 | IFNOT | if the raw word at A is zero, jump by `b` |
| 51–59 | CALL0 … CALL8 | call the function whose index is in A, with (op − 51) arguments already in the parameter slots. Function 0 is the error "NULL function". A negative first statement is builtin −first: it reads its arguments from the parameter slots and the argument count, writes its result to the return slot, and the next statement runs; a builtin number past the table, or one the game left empty, is an error naming the number. Otherwise enter the function |
| 60 | STATE | on the entity in the global `self`: `nextthink` = `time` + 0.1 (the float `time` widened to double, 0.1 added in double, rounded back to float), `frame` = A, `think` = B (a function value) |
| 61 | GOTO | jump by `a` statements |
| 62 | AND | C = A true and B true (both read; no short circuit in the VM) |
| 63 | OR | C = A true or B true |
| 64 | BITAND | C = (A truncated to an integer) & (B truncated) as a float |
| 65 | BITOR | the same with OR |

**Truncation** for BITAND and BITOR: toward zero, as a 32-bit signed integer. Out of range or
NaN is undefined in the server's language; on the machines it runs on the result is the integer
−2147483648. **Ours**: that value, so a mod sees the same flags everywhere.

**Unchecked in the server, checked in ours**: an entity value that is not a multiple of the
entity size or past the last entity; a field past the entity size; a pointer outside the entity
array; a call to a function index past the last; a global operand outside the globals (the
checker refuses those at load). Each is an error with the stack trace, never a silent read.

Any other opcode is the error "bad opcode N".

## The engine-side interface

The multiplayer layout the crc 54730 names. The program declares these first, in this order, so
their offsets are fixed; the engine reads and writes them by offset.

**Globals** (one word each unless marked v, a vector of three; offset in words):

| Offset | Global | Offset | Global |
| --- | --- | --- | --- |
| 28 | self (entity) | 66 | trace_allsolid |
| 29 | other (entity) | 67 | trace_startsolid |
| 30 | world (entity) | 68 | trace_fraction |
| 31 | time | 69 | trace_endpos v |
| 32 | frametime | 72 | trace_plane_normal v |
| 33 | newmis (entity) | 75 | trace_plane_dist |
| 34 | force_retouch | 76 | trace_ent (entity) |
| 35 | mapname (string) | 77 | trace_inopen |
| 36 | serverflags | 78 | trace_inwater |
| 37 | total_secrets | 79 | msg_entity (entity) |
| 38 | total_monsters | 80 | main (function) |
| 39 | found_secrets | 81 | StartFrame (function) |
| 40 | killed_monsters | 82 | PlayerPreThink (function) |
| 41–56 | parm1 … parm16 | 83 | PlayerPostThink (function) |
| 57 | v_forward v | 84 | ClientKill (function) |
| 60 | v_up v | 85 | ClientConnect (function) |
| 63 | v_right v | 86 | PutClientInServer (function) |
| | | 87 | ClientDisconnect (function) |
| | | 88 | SetNewParms (function) |
| | | 89 | SetChangeParms (function) |

Unmarked globals are floats. The hooks are function values the program stores in these
globals; the engine calls whatever they hold. The program's own globals usually follow from 90 (a compiler lays them out in its order). The
building session checks the table against a compiled program's global definitions, which name
each one.

**Fields** (from word 0 of an entity, in order; v = vector): modelindex, absmin v, absmax v,
ltime, lastruntime, movetype, solid, origin v, oldorigin v, velocity v, angles v, avelocity v,
classname (string), model (string), frame, skin, effects, mins v, maxs v, size v, touch (function),
use (function), think (function), blocked (function), nextthink, groundentity (entity), health,
frags, weapon, weaponmodel (string), weaponframe, currentammo, ammo_shells, ammo_nails,
ammo_rockets, ammo_cells, items, takedamage, chain (entity), deadflag, view_ofs v, button0,
button1, button2, impulse, fixangle, v_angle v, netname (string), enemy (entity), flags,
colormap, team, max_health, teleport_time, armortype, armorvalue, waterlevel, watertype,
ideal_yaw, yaw_speed, aiment (entity), goalentity (entity), spawnflags, target (string),
targetname (string), dmg_take, dmg_save, dmg_inflictor (entity), owner (entity), movedir v,
message (string), sounds, noise, noise1, noise2, noise3 (strings). Everything not marked is a
float. The offsets follow by counting words (a vector three); the program's own fields come
after.

**Optional fields** the engine looks up **by name** after loading (absent = the feature is
off): `items2`, `maxspeed`, `gravity`, `movement`, `vw_index`, `hideentity`, `trackent`,
`visclients`, `hideplayers`, `teleported`. What each does is M18p's (`builtins.md`).

**Optional hooks** found **by name** among the functions (absent = not called):
`SpectatorConnect`, `SpectatorThink`, `SpectatorDisconnect`, `ChatMessage`, `UserInfo_Changed`,
`ConsoleCmd`, `UserCmd`, `localinfoChanged`, `GE_ClientCommand`, `GE_PausedTic`,
`GE_ShouldPause`, and `timeofday`. Their arguments and results are M18p's.

**Lookups by name** (for those and for the map's entities): fields and globals by their
definitions' names, functions by name; the first match in file order wins.

## The map's entities

At map start the engine reads the map's entity text (a sequence of `{ "key" "value" … }`
blocks) and for each block:

1. The first block fills the world (entity 0); each later one is a newly allocated entity.
2. Each key names a field by name and is parsed by the field's type: a string is stored as a new
   engine string with `\n` turned into a newline (a backslash followed by anything else becomes
   a backslash); a float by the usual decimal reading; a vector as three numbers separated by
   spaces; an entity as an entity number; a field by a field's name (an unknown one is a parse
   error that stops the map); a function by a function's name (the same). Keys starting with
   `_` are ignored. The key `angle` becomes `angles` with the value `0 <value> 0`; `light`
   becomes `light_lev`. `alpha` and `colormod` go to the engine's own per-entity extension
   values, not to a field (M18p). An unknown key prints "<key> is not a field" and is skipped.
3. A block with no keys leaves the entity free.
4. In deathmatch (the `deathmatch` setting not 0) an entity whose `spawnflags` has bit 2048
   (not in deathmatch) is freed; otherwise skill 0 frees bit 256, skill 1 bit 512, skill 2 and
   up bit 1024. Each one freed is counted as inhibited.
5. An entity with no `classname` is freed with a message; otherwise the function named by its
   classname is called with `self` set to the entity (`time` is the server's time). No such
   function: freed with a message.

## Allocating and freeing entities

- **Allocate**: look from entity 33 (after the world and the 32 client slots) up to the highest
  in use for a free one that was freed before the server's second 2 or more than 0.5 s ago;
  clear it (every field zero) and return it. None: extend the highest-in-use by one. At the
  2048 limit, warn and reuse the last entity (unlinked from the world first).
- **Free**: unlink it from the world; mark it free and note the time; clear model, takedamage,
  modelindex, colormap, skin, frame, health, classname, origin, angles and solid to zero and
  set nextthink to −1. Other fields keep their values until it is allocated again (mods read
  a freed entity's fields and rely on that).

## Builtins

Called by number (the negative first statement). The numbered list of the modern multiplayer
server, its extension builtins and the extension query are M18p's, specified in `builtins.md`.
For M18c the VM provides the table a game fills (number → native function, with the argument
count it was called with), an empty slot being the error "builtin N is not provided by this
game", and the extension registry: a named extension groups builtins, fields and hooks so the
game can answer which extensions it offers.

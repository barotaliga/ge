# QuakeC bytecode, specified

The compiled QuakeC a mod ships (`progs.dat`, `qwprogs.dat`), in our words (README.md, the rules).
Little endian throughout. Sources as in `formats.md`.

## The file

Header: version (i32, 6), a CRC of the engine-side field layout (i32), then six (offset, count)
pairs: statements, global definitions, field definitions, functions, strings (offset and size in
bytes), globals; then the entity size in 32-bit words (spec).

| Record | Size | Fields |
| --- | --- | --- |
| statement | 8 | opcode (u16), a, b, c (u16 global offsets) |
| definition | 8 | type (u16; bit 15 = saved with a game), offset (u16), name (i32 string offset) |
| function | 36 | first statement (i32; negative = builtin number), first parameter's global (i32), locals (i32), profile (i32), name (i32), file (i32), parameter count (i32), parameter sizes (8 u8) |

**Types**: 0 void, 1 string, 2 float, 3 vector, 4 entity, 5 field, 6 function, 7 pointer
(spec). Every value is one or three 32-bit words; floats are single precision.

**Globals**: offset 1 holds the return value (3 words), offsets 4, 7, … 25 the eight parameters
(3 words each), offset 28 onward the program's own globals and the engine-side globals the
progs declares in a fixed order (spec, verify the reserved range).

## The opcodes (spec)

| Range | Opcodes |
| --- | --- |
| 0 | DONE |
| 1–9 | MUL_F, MUL_V (dot product), MUL_FV, MUL_VF, DIV_F, ADD_F, ADD_V, SUB_F, SUB_V |
| 10–19 | EQ_F, EQ_V, EQ_S, EQ_E, EQ_FNC, NE_F, NE_V, NE_S, NE_E, NE_FNC |
| 20–23 | LE, GE, LT, GT |
| 24–29 | LOAD_F, LOAD_V, LOAD_S, LOAD_ENT, LOAD_FLD, LOAD_FNC (entity.field read) |
| 30 | ADDRESS (a pointer to entity.field, for the STOREP family) |
| 31–36 | STORE_F, STORE_V, STORE_S, STORE_ENT, STORE_FLD, STORE_FNC |
| 37–42 | STOREP_F … STOREP_FNC |
| 43 | RETURN |
| 44–48 | NOT_F, NOT_V, NOT_S, NOT_ENT, NOT_FNC |
| 49–50 | IF, IFNOT (a relative jump in b) |
| 51–59 | CALL0 … CALL8 (the parameter count) |
| 60 | STATE (the frame and think function shorthand) |
| 61 | GOTO (relative in a) |
| 62–65 | AND, OR, BITAND, BITOR |

Exact semantics to specify one by one with M18c, each with a test in the VM's assembler: string
comparison (by content or by offset: verify), NOT_S of an empty string, the runaway-loop limit,
what writing a field of the world entity does, the frame and think fields STATE writes.

## The engine-side interface

The fields and globals the engine reads and writes in every entity (origin, velocity, angles,
model and modelindex, solid, movetype, flags, think and nextthink, the touch and use functions,
health, frame, skin, effects, owner, …) and the entry points the server calls (StartFrame,
PlayerPreThink, PlayerPostThink, ClientConnect, ClientDisconnect, PutClientInServer,
SetNewParms, SetChangeParms, ClientKill, the spectator functions): the multiplayer and the
single-player progs differ. Written as two tables (name, type, order) with M18c, from the
language reference and checked against what a compiled free mod declares (`ge qc disasm`).

## Builtins

Called by number (the negative first statement). The two numbered lists (multiplayer and
single-player) and the extension builtins are written here with M18c: number, name,
parameters, result and behaviour in our words, one row each.

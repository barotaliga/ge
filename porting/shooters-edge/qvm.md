# The compiled game module (.qvm), specified

The match mod KTX ships as one compiled module, `qwprogs.qvm`: C compiled to a small
stack-machine bytecode, run by the server in a sandboxed VM and talking to it through numbered
system calls. This note is what our server needs to run that file **unchanged**, the way the
modern multiplayer server runs it, in our words (README.md, the rules). Little endian throughout.

**Sources.** A research session read the modern multiplayer server's VM and module-interface
source and the mod's interface headers, and measured the shipped file
(`qwprogs.qvm`, 1 559 016 bytes, SHA-256 `97fbf817…836d0`, the mod's version string
"1.46-dev") with a throwaway decoder that was not committed. The building session takes it from
here and does not look further. Where the server's behaviour is undefined, unchecked or a
crash, this note says so and names what our VM does instead under **Ours**.

What is already specified elsewhere and reused unchanged: the game loop and physics
(`gameloop.md`), the move (`movement.md`), the messages (`protocol.md`), the console's words and
tokenizer (`configs.md`), and what each classic builtin does (`builtins.md`). The QC VM
(`progs.md`, `plugins/progs`) is a different machine; § 6 lists what is new.

---

## 1. The file and the VM's memory

### Header

Eight little-endian i32 (32 bytes) for the version-1 magic, nine (36 bytes) for the version-2
magic, in this order:

| # | Field | Rule |
| --- | --- | --- |
| 0 | magic | `0x12721444` (version 1) or `0x12721445` (version 2); anything else is refused ("bad file magic") |
| 1 | instruction count | the number of instructions in the code segment |
| 2 | code offset | file offset of the code segment; must be < the file size |
| 3 | code length | bytes; must be > 0 and code offset + length ≤ file size |
| 4 | data offset | must be < file size **and equal to code offset + code length** |
| 5 | data length | bytes of initialised 32-bit words; data offset + length ≤ file size |
| 6 | lit length | bytes of initialised bytes (strings), which follow the data in the file |
| 7 | bss length | bytes of zero-filled memory after lit; not stored in the file |
| 8 | jump-target length | version 2 only: bytes of the jump-target table that follows lit |

The file must end exactly where its segments end: data offset + data length + lit length (+ the
jump-target length for version 2) **equals** the file size; otherwise refused ("bad lit
segment length"). A file shorter than its header is refused ("truncated image header").

### Segments

- **Code**: `instruction count` instructions back to back, each one opcode byte followed by its
  operand: 4 bytes (a little-endian i32) for the opcodes marked 4 in § 2, 1 byte (unsigned) for
  ARG, none for the rest. An opcode byte ≥ 60 is refused ("bad opcode XX at offset N"); running
  past the code length is refused. Operands that name code (call and jump targets) are
  **instruction indices**, not byte offsets, so a loader decodes the code into an array.
- **Data**: little-endian 32-bit words (on a big-endian host each word is byte-swapped at load).
- **Lit**: raw bytes (string literals); never swapped.
- **BSS**: zeros. The compiler's tool chain reserves the module's 64 KiB program stack at the
  end of bss, so bss length already includes it.
- **Jump targets** (version 2): `jump-target length` rounded down to a multiple of 4, as i32
  instruction indices: every instruction a computed jump (a `switch` table) may land on. Only an
  optimising translator needs them; an interpreter may ignore the segment. (The reference
  validates each index against the code and silently ignores the whole table when one is bad.)

### The memory image

The VM has one flat byte-addressed memory; a VM address is a byte offset into it, and **address
0 is the null pointer** (every system call treats 0 as "no pointer").

1. `exact length` = data length + lit length + bss length.
2. `memory size` = the smallest power of two ≥ exact length + 32 768 (a 32 KiB margin for
   modules that overrun their stack). A memory size ≥ 2³¹ is refused. `data mask` =
   memory size − 1.
3. The memory is zeroed; the data segment is copied to address 0, the lit segment directly after
   it (address = data length); bss and everything up to the memory size stay zero.
4. **The program stack** grows down from the top: its pointer starts at `memory size` (one past
   the last byte). Its bottom limit is memory size − 65 536 − 32 768; an ENTER that takes the
   pointer **at or below** that limit is the error "VM programStack overflow".
5. The reference allocates 1024 bytes beyond the memory size as slack for its unmasked local
   loads (§ 2, traps); a correct module never touches them.

**A fresh image every map.** The server unloads the module at the end of each map and loads the
file again for the next, so all module memory starts from the file's values each map (that is how
the mod's static state resets). Only cvars, server info, local info and the client slots survive.

**Server info**: on load the server sets the star key `*progs` to the file's 16-bit CRC (the
classic CCITT CRC: polynomial 0x1021, initial value 0xFFFF, no final xor, over every byte of the
file) printed as a decimal number. For the measured file that is `27`.

**Symbols** (optional): a text file `<name>.map` beside the module, lines of
`<segment hex> <value hex> <name>`; segment 0 lines name code (the value is an instruction
index). Used only for stack traces and the profile. The shipped mod has none.

---

## 2. The instruction set

### The machine

- **pc**: an instruction index.
- **The operand stack**: 32-bit words, private to one host call (each entry from the host gets a
  fresh, empty one). In the table, `T` is the top word and `N` the one below it; "pop 2, push x"
  means both are removed and x pushed. Words are reinterpreted freely as signed int, unsigned
  int or IEEE single float (the bits never change on a reinterpretation).
- **PS, the program stack pointer**: a byte address in VM memory; each function's frame lives at
  [PS, PS + frame size).
- All memory accesses are little-endian and may be unaligned.

### The opcodes

Operand: 4 = a 4-byte i32 `v`, 1 = a 1-byte unsigned `v`, — = none.

| # | Name | Op | Does |
| --- | --- | --- | --- |
| 0 | UNDEF | — | nothing (never emitted) |
| 1 | IGNORE | — | nothing |
| 2 | BREAK | — | nothing except count a debugger break |
| 3 | ENTER | 4 | function entry: PS −= v (the frame size in bytes, a multiple of 4, 0 ≤ v < 65 536); then the program-stack and operand-stack overflow checks (below) |
| 4 | LEAVE | 4 | function exit: PS += v; read the i32 at memory[PS]: the caller's saved pc. If it is −1, the host call is finished (below); otherwise pc = it (out of range is the error "program counter out of range in OP_LEAVE"). The operand stack is not touched: the function's one result word stays on top |
| 5 | CALL | — | target = T. Store the index of the next instruction as an i32 at memory[PS] (the **caller's** frame, offset 0). Target < 0: a **system call** (below); T is replaced by its result and execution continues after the CALL. 0 ≤ target < instruction count: pop T and jump to it. Anything else: the error "program counter out of range in OP_CALL" |
| 6 | PUSH | — | push one word (its value is undefined in the reference; **Ours** 0). Emitted before LEAVE in a function that returns nothing |
| 7 | POP | — | pop 1 (discard an unused result) |
| 8 | CONST | 4 | push v |
| 9 | LOCAL | 4 | push PS + v (the address of a local, a parameter or an outgoing argument) |
| 10 | JUMP | — | pop T; pc = T as an instruction index (unsigned ≥ instruction count is the error "program counter out of range in OP_JUMP") |
| 11 | EQ | 4 | pop 2; if N == T (as i32) pc = v |
| 12 | NE | 4 | pop 2; if N != T, pc = v |
| 13–16 | LTI, LEI, GTI, GEI | 4 | pop 2; signed compare N <, ≤, >, ≥ T; if true pc = v |
| 17–20 | LTU, LEU, GTU, GEU | 4 | the same, unsigned |
| 21–22 | EQF, NEF | 4 | pop 2; IEEE single compare N == T, N != T (NaN equals nothing; −0 equals 0); if true pc = v |
| 23–26 | LTF, LEF, GTF, GEF | 4 | pop 2; IEEE compare N <, ≤, >, ≥ T (false with a NaN); if true pc = v |
| 27 | LOAD1 | — | T = the byte at address T, **zero-extended** |
| 28 | LOAD2 | — | T = the 16-bit value at address T, zero-extended |
| 29 | LOAD4 | — | T = the 32-bit word at address T |
| 30 | STORE1 | — | pop 2; store the low 8 bits of T at address N |
| 31 | STORE2 | — | pop 2; store the low 16 bits of T at address N |
| 32 | STORE4 | — | pop 2; store T at address N |
| 33 | ARG | 1 | pop T; store it as a word at PS + v (v ≥ 8: an outgoing argument slot in the current frame) |
| 34 | BLOCK_COPY | 4 | pop 2; copy v bytes from address T (source) to address N (destination) |
| 35 | SEX8 | — | T = the low 8 bits of T sign-extended to 32 |
| 36 | SEX16 | — | T = the low 16 bits of T sign-extended |
| 37 | NEGI | — | T = −T (two's complement, wrapping) |
| 38 | ADD | — | pop 2, push N + T (wrapping 32-bit) |
| 39 | SUB | — | pop 2, push N − T |
| 40 | DIVI | — | pop 2, push N ÷ T signed, truncated toward zero |
| 41 | DIVU | — | pop 2, push N ÷ T unsigned |
| 42 | MODI | — | pop 2, push the signed remainder (the sign of N) |
| 43 | MODU | — | pop 2, push the unsigned remainder |
| 44 | MULI | — | pop 2, push the low 32 bits of N × T |
| 45 | MULU | — | the same (the low 32 bits are identical) |
| 46 | BAND | — | pop 2, push N & T |
| 47 | BOR | — | pop 2, push N \| T |
| 48 | BXOR | — | pop 2, push N ^ T |
| 49 | BCOM | — | T = ~T |
| 50 | LSH | — | pop 2, push N << T |
| 51 | RSHI | — | pop 2, push N >> T arithmetic (sign-filling) |
| 52 | RSHU | — | pop 2, push N >> T logical (zero-filling) |
| 53 | NEGF | — | T = −T as a float (flips the sign bit, also of a NaN or 0) |
| 54 | ADDF | — | pop 2, push N + T in IEEE single precision |
| 55 | SUBF | — | pop 2, push N − T |
| 56 | DIVF | — | pop 2, push N ÷ T (a division by zero gives ±inf or NaN, never an error) |
| 57 | MULF | — | pop 2, push N × T |
| 58 | CVIF | — | T = (signed int T) converted to float, rounded to nearest even |
| 59 | CVFI | — | T = (float T) converted to a signed int, truncated toward zero |

Float arithmetic is IEEE single precision with round-to-nearest-even and no wider intermediate:
the reference runs it on SSE scalar single-precision instructions, both interpreted and
translated. **Ours**: the same in C++ `float`, no FMA contraction, no fast-math (the engine's FP
flags already guarantee it).

**Where C leaves it undefined** (the reference simply runs the host's instruction):
- Shift counts outside 0..31: the host masks the count to its low 5 bits. **Ours**: mask with 31.
- CVFI of a NaN or of a value outside the i32 range: the host gives −2 147 483 648. **Ours**: that
  value (the same rule as `progs.md`'s BITAND).
- DIVI/MODI/DIVU/MODU by zero, and DIVI/MODI of −2 147 483 648 by −1: the host process dies (a
  hardware divide fault). **Ours**: a VM error ("division by zero" / "division overflow") with
  the stack trace. The measured file never divides by a constant zero.

### Functions, frames and the calling convention

The compiler's convention, which the VM only has to honour mechanically:

- A function begins with ENTER *n* and ends (possibly at several exits) with LEAVE *n*, the same
  *n*. After ENTER, the function's frame is [PS, PS + *n*).
- Frame offset 0 is where a CALL made by this function stores its return pc; offset 4 is where a
  system call made by this function gets its number written (below). Outgoing arguments go to
  offsets 8, 12, 16, … (ARG 8, ARG 12, …), one word each; a structure argument is passed as a
  pointer.
- The callee sees its own parameters in its caller's frame: parameter *i* is at its own
  PS + *n* + 8 + 4*i* (reached with LOCAL *n* + 8 + 4*i*).
- A function's result is the one word it leaves on the operand stack at LEAVE (PUSH supplies a
  dummy for a function without one). Seen from the caller, CALL replaces the target address on
  the operand stack with that result; the caller POPs it when unused.
- Recursion needs nothing special: frames stack downward in memory.

### System calls

A CALL whose target T is negative is a call into the host: **the call number is −T − 1** (so
address −1 is system call 0, −2 is 1, …, −257 is 256). The VM:

1. writes the call number as an i32 at PS + 4 (the current frame's offset 4);
2. hands the host an argument array: element 0 is the word at PS + 4 (the number), element *k*
   (k ≥ 1) is the word at PS + 4 + 4*k* — that is, the caller's outgoing argument *k* − 1. The
   host reads as many as the call takes (the reference copies 16 words regardless; reading
   unused stale slots is harmless);
3. records PS − 8 as the starting stack for any **re-entrant host call** made while this system
   call runs (below);
4. replaces T with the 32-bit result (a result wider than 32 bits is truncated) and continues
   at the instruction after the CALL.

Argument words are typed by the call (§ 4): an int, an entity number, a VM address (0 = null),
or a float **by its bits** (the module's compiler passes a float argument as its 32-bit pattern;
the host reinterprets it, and a float result is returned the same way).

### The host calling into the module

The module's entry function (the mod's `vmMain`) is **instruction 0**, which must be an ENTER.
To call it with a command number and up to three int arguments (the reference never passes
more; § 3):

1. PS = the recorded starting stack: the memory size for an outermost call, or the value step 3
   above recorded when the host is called back from inside a system call.
2. PS −= 24 and write six words at [PS, PS + 24): −1 (the saved pc that ends the call), 0, the
   command number, argument 0, argument 1, argument 2 (unused arguments 0).
3. pc = 0 with an empty operand stack, and run until a LEAVE reads the saved pc −1.
4. The operand stack must then hold **exactly one** word, else the error "Interpreter error:
   opStack = N". That word is the call's result (an int).
5. Restore the recorded starting stack to what it was before this call (so calls nest).

The entry function reads its arguments as ordinary parameters: the command at its PS + *n* + 8,
argument *k* at PS + *n* + 12 + 4*k*. (It declares thirteen parameters but reads only the first
three; the words past the six-word block are whatever lies above it and must never matter.)

**Re-entry is required.** Several system calls call back into the module before they return:
spawn (clears the new entity through export 20), walkmove and movetogoal (touch functions run
during the step), executecmd and readcmd (console commands can be the `mod` command), add_bot
and remove_bot (connect, put-in-server, disconnect), setuserinfo (userinfo changed). A nested
host call starts its frames below the system call's frame (step 3), gets its own operand stack,
and when it returns the outer call continues untouched.

### Limits and run-time checks

| Check | The reference | **Ours** |
| --- | --- | --- |
| program stack | ENTER below the stack bottom (§ 1) is the error "VM programStack overflow" | the same |
| operand stack | 512 words per host call; at ENTER, the current depth plus the function's maximum depth must stay below 511, else "VM opStack overflow" | the same limit (or larger); KTX's deepest function is far below it |
| loads and stores | the interpreter masks every LOAD/STORE/BLOCK_COPY address with the data mask (so a wild address wraps inside memory); LOCAL+LOAD, ARG and the frame words are not masked. The translator, at its default setting, neither masks nor checks | **every** access is checked: address + size ≤ memory size, else a VM error naming the opcode, the address and the function. A correct module never notices the difference |
| block copy | the count is clipped so neither range passes the end of memory | the count must fit both ranges, else a VM error. Copies may overlap (use a move) |
| jump / call / leave targets | range-checked as in the table | the same |
| run-away | none | none required; an optional statement budget per host call is allowed if it is generous (KTX runs long loops at map start) |

Every VM error stops the module with the message, the faulting instruction, and a stack trace
(walk the saved-pc words up the frames; with no symbol file name functions by their ENTER's
instruction index). In the reference every such error ends the server (a fatal error).
**Ours**: the same facts in the engine's diagnostics shape, and the game decides what a fault ends
(as `progs.md` § Errors).

### Load-time checks (the reference's verifier)

The reference refuses the module at load if any of these fails; **Ours** runs the same checks (a
refusal names the rule, the instruction index and the value):

- Summing each opcode's static stack effect over the whole code never goes below 0 and never
  reaches 120 bytes (30 words). Static effects in bytes: ENTER 0, LEAVE −4, CALL 0, PUSH +4,
  POP −4, CONST +4, LOCAL +4, JUMP −4, the 16 compare-and-branch −8, LOAD1/2/4 0, STORE1/2/4 −8,
  ARG −4, BLOCK_COPY −8, SEX8/16 0, NEGI 0, the binary integer and float ops −4, BCOM 0, NEGF 0,
  CVIF/CVFI 0. "The static depth" of an instruction below is this running sum before it.
- Every ENTER has static depth 0, a frame size 0 ≤ v < 65 536 and a multiple of 4, is not inside
  another function, and is followed (later) by a PUSH immediately followed by LEAVE (the
  function's end).
- Every LEAVE has the frame size of its function's ENTER and static depth 4.
- A compare-and-branch has static depth 8 and its target lies inside the current function and
  has static depth 0.
- A JUMP has static depth 4; when the previous instruction is a CONST, the constant is the target:
  inside the current function, static depth 0, not an ENTER, not the CONST itself (a self loop).
- A CALL has static depth ≥ 4; when the previous instruction is a CONST ≥ 0, that target is
  < the instruction count, is an ENTER, and is not 0 (the module may not call its own entry).
- ARG's operand is ≥ 8, a multiple of 4, and ≤ the frame size − 4.
- A LOCAL is inside a function; when followed by a LOAD or an ARG, its operand is ≥ 8 and < the
  frame size + 256.
- A LOAD4/LOAD2/LOAD1 right after a CONST reads inside the exact length.
- A BLOCK_COPY's count is < the exact length.
- The last instruction is a LEAVE.

---

## 3. The host calling the module: the exports

Each export is a call of the entry function (§ 2) with a command number and up to three int
arguments. Before every call the server writes the globals the QC path writes before the
matching hook (`time`, `self`, `other`, … in the module's global block, § 5); the module reads
its arguments from there. The result is an int.

| # | Export | Args | Result | Called |
| --- | --- | --- | --- | --- |
| 0 | init | level time in ms, random seed | the VM address of the game data block (§ 5); 0 = refuse (the server stops: "gamedata == NULL") | once per map, right after loading, before any entity exists |
| 1 | load entities | | ignored | once per map, after the first start frame, while the map is loading; the module reads the entity text with system call 3 and spawns everything itself |
| 2 | shutdown | | ignored | before the module is unloaded: at every map change (after set-change-parms), and at server shutdown |
| 3 | client connect | 1 = spectator, 0 = player | ignored | `self` = the client; where the QC path calls ClientConnect / SpectatorConnect |
| 4 | put client in server | 1 = spectator, 0 = player | ignored | `self` = the client; right after client connect (PutClientInServer; the spectator form has no QC counterpart) |
| 5 | client userinfo changed | 0 = before, 1 = after | before: non-zero **refuses** the change | `self` = the client; around every userinfo change a client asks for (`setinfo`) and every system call 91. The console's words are the change (§ 4, the console's words) |
| 6 | client disconnect | 1 = spectator, 0 = player | ignored | `self` = the client; ClientDisconnect / SpectatorDisconnect |
| 7 | client command | | non-zero = handled | `self` = the client, `time`; every client command that is overridable or not the server's (the rules of `builtins.md` § Client commands), and `kill` (the QC ClientKill). The console's words are the command |
| 8 | client prethink | 0 | ignored | `self` = the player; PlayerPreThink, players only |
| 9 | client think | | | never called by the reference |
| 10 | client postthink | 1 = spectator, 0 = player | ignored | `self`; PlayerPostThink for a player, SpectatorThink for a spectator |
| 11 | start frame | server time in ms (time × 1000 truncated), 1 = a bot frame | ignored | `self` = `other` = world, `time` = server time; StartFrame at the head of each physics frame with 0; with 1 at the head of each bot frame (below) |
| 12 | set change parms | | ignored | `self` = the client; SetChangeParms |
| 13 | set new parms | | ignored | SetNewParms |
| 14 | console command | | ignored | the server console command `mod <words>`; `self` = the client whose address sent it (a remote console), else world; `other` = world; both restored after. The words are the console's, word 0 is `mod`. (Unknown console commands are **not** offered to the module, unlike QC's ConsoleCmd) |
| 15 | edict touch | | ignored | `self`, `other`, `time` as the QC path sets them; wherever the QC path would call the entity's touch function (the module dispatches through its own function pointer) |
| 16 | edict think | | ignored | wherever the QC path would call the entity's think function |
| 17 | edict blocked | | ignored | wherever the QC path would call the entity's blocked function |
| 18 | client say | 1 = say_team, 0 = say | non-zero = handled (the server prints nothing) | `self` = the client, `time`; the QC ChatMessage moment. The console's words are the say command |
| 19 | paused tic | milliseconds paused so far | ignored | every server frame while paused (QC's GE_PausedTic) |
| 20 | clear edict | | ignored | `self` = the entity, restored after; only in **reference mode** (§ 5): every time the server clears an entity (allocation, a client's edict set up, a bot added) and for every entity slot right after init |
| 200 | entity send | send flags low, high | | only for a server with client-side QC; ours never calls it |
| 201 | client request | | | the same; never |

The server's tests that decide whether to call 15–17 read the entity's function words (§ 5), not
the module's pointers: touch when the `touch` word is non-zero and the entity is not
non-solid; blocked when the `blocked` word is non-zero; think whenever `nextthink` comes due,
whatever the `think` word holds. Spectator connect, prethink for spectators and the QC hooks
found by name (`UserCmd`, `GE_ClientCommand`, `ConsoleCmd`, `UserInfo_Changed`, …) do not
exist for a module: the exports above replace them.

### The order at a map's start

1. (Previous map) set change parms for each client, shutdown, unload.
2. Load the file (§ 1) and set `*progs`. `sv_pr2references` is set to 0.
3. **init**(1000, seed). The level time is the server's clock at spawn (1.0 s → 1000). The
   reference seeds with the wall clock in seconds. **Ours**: the server's seeded value (G1).
   During init the module calls api_version, cvars, local commands, the extension mapping and
   file listings; it must not be handed entities (none exist yet, and the precache state is not
   "loading").
4. Read the game data block (§ 5). Refuse an API version outside 16..17 ("Incorrect API
   version"). Turn on reference mode if the version is ≥ 15 **and** `sv_pr2references` is now
   non-zero (KTX sets it to 1 during init, so reference mode is always on for it). The entity
   limit becomes min(2048, the block's maximum); later lowered to 512 when the server's extended
   limits are off.
5. For every entity slot below the limit: bind it to the module's memory (§ 5) and, in reference
   mode, call **clear edict** with `self` = it.
6. Look up the extension fields by name (§ 5).
7. Set up the world entity: modelindex 1, solid BSP, movetype push, `impulse` = the server's
   version number, `items` = the QC builtin table's size − 1 (the value the QC path writes), `serverflags` global. **No string field is
   written** by the server in module mode (the model, netname, targetname and mapname the QC path
   writes are skipped; the module asks for them by infokey, below).
8. The map state becomes "loading"; **start frame**(time ms, 0) with `time` = 1.0.
9. **load entities**: the entity text is the map's own, or a `.ent` file when the server's entity
   files setting finds one. The module parses it itself.
10. The state becomes active; two physics frames run (`gameloop.md`).

### Each server frame

As `gameloop.md` § The server's frame, with the hooks replaced by exports. In addition, after
the physics frame (when not paused) the reference runs **bot frames**: at the server's packet
rate setting (`maxfps`, default 77; below 20 or above 1000 means 77), with `frametime` = 1 / rate, it
calls start frame (time ms, 1) and then runs each bot client's stored command (system call 81)
through the same player command path as a real client's (prethink, the move, postthink), then
clears the stored command. While paused it calls paused tic instead of the physics.

### A client's life

- Entering: the server clears the client's entity (in reference mode: clear edict), sets
  `colormap` = its number, `team` = 0, the `gravity` field 1 and `maxspeed` field to the server's
  maximum speed; set new parms; copies the spawn parms; connect(0/1) then put in server(0/1) with
  `time` and `self`. (The reference also writes the client's VIP level into global word 4, the
  QC first parameter slot; in the module's block that word is padding. **Ours**: skip it.)
- `join` / `observe`: disconnect(old kind), entity cleared again, set new parms, connect and put
  in server with the new kind.
- Leaving (spawned clients only): disconnect(0/1) with `self` = it.

---

## 4. The module calling the host: the system calls

Conventions for the argument column: **n** an entity number (0 = world); **e*** a VM address of
an entity record (ents + n × entity size, § 5); **s** a VM address of a NUL-terminated string;
**buf, size** a VM address and a byte count the host writes into; **f** a float passed by its
bits; **i** an int; **v** three consecutive float arguments (x, y, z); **p** a VM address of
three floats. Results are ints unless marked f (float bits) or e* / n.

Common rules:
- **An entity number** is checked by the reference: 0 ≤ n < the entity limit, else the fatal
  error "EDICT_NUM: bad number". **An entity address** is turned into a number by subtracting
  ents and dividing by the entity size; outside 0 ≤ n < the number of entities in use is "bad
  pointer". **Ours**: the same checks, and also refuse an address that is not exactly on an
  entity's start.
- **A string or pointer argument** is the VM address; the reference masks it and does not check
  that the string ends inside memory, and most calls crash on a null. **Ours**: every string is
  read with a check that it ends inside memory (else a VM error naming the call); **a null string
  reads as the empty string** (the module never relies on the crash).
- **A buffer the host writes** is bounds-checked where the table says "checked": address and size
  must each be ≤ the data mask and address + size ≤ the data mask, else the fatal error "program
  tried to bypass data segment bounds". **Ours**: check every buffer the host writes or reads,
  everywhere (address + size ≤ memory size).
- **Strings the host keeps** (precache names, light style patterns, the model a setmodel stores)
  are kept by the reference as pointers into module memory. **Ours**: copy the text at the call
  (KTX passes literals or buffers it never rewrites, so the result is the same).
- "As builtin N" means the behaviour of `builtins.md`'s builtin N, with the argument differences
  given.
- **Calls** = the number of call sites in the measured file (§ 7); 0 = never called.

### General

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 0 | api_version | → i | the server's interface version: the reference answers 17. KTX needs ≥ 16. **Ours**: 16 (17 only promises the client-side QC exports, which ours does not have) | 1 |
| 1 | dprint | s | print to the server console, only when developer messages are on | 2 |
| 2 | error | s | the module's fatal error: print it, then stop the module ("Program error: …"); the reference ends the server | 1 |
| 3 | entity_token | buf, size → i | (checked) read the next token of the map's entity text into buf (truncated to size − 1 and terminated); 1 if a token was read, 0 at the end (buf then empty). The tokenizer: skip spaces, tabs, CR and LF; `//` skips to the end of the line; a `"` starts a token that runs to the next `"` or the end (quotes dropped); otherwise a token runs to the next space, tab, CR, LF or the end (so `{` and `}` are tokens only when spaced, as map text always is); at most 1023 characters kept | 3 |
| 75 | conprint | s | print to the server console directly (never redirected, not logged) | 1 |

### Entities

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 4 | spawn | → n | allocate an entity as `progs.md` § Allocating (search from 33; the 0.5 s / 2 s reuse rule; the 2048 warning). Clearing zeroes all entity-size bytes of the record and its extension values, then (reference mode) calls **clear edict** with `self` = it (a re-entrant call) | 1 |
| 5 | remove | n | free it as `progs.md` § Allocating: unlink; clear its extension values; mark free and note the time; zero the words `model`, `takedamage`, `modelindex`, `colormap`, `skin`, `frame`, `health`, `classname`, `origin`, `angles`, `solid`; `nextthink` = −1. (In reference mode the zeroed `model` and `classname` words are the references, § 5; the module restores them on reuse.) Client entities are not refused by the server | 1 |
| 9 | setorigin | n, v | as builtin 2 | 1 |
| 10 | setsize | n, v mins, v maxs | as builtin 4 | 1 |
| 11 | setmodel | n, s | as builtin 3 (must be precached, else the error "no precache: s"; inline `*` models set the size). The model string is stored **through the reference** (§ 5): in reference mode the VM address s is written into the module word the entity's `model` reference names; otherwise into the `model` word itself | 1 |
| 28 | nextent | n → n | the next entity after n that is not free, below the number in use; 0 after the last | 1 |
| 88 | nextclient | e* → e* | the next client slot after the given entity (1..32) whose client is fully in the game ("spawned"); 0 when none. The world's address starts the walk | 4 |
| 73 | find | e* start, i fofs, s → e* | the next entity after start (in number order, below the number in use) that is not free whose word at **byte offset fofs** of its record is a non-zero VM address of a string equal to s by content; 0 if none. A null s is an error. (fofs is an offset into the module's own record, where KTX keeps real string pointers; references are not followed here) | 2 |
| 23 | findradius | e* start, p org, f rad → e* | **one** step, not a chain: the next entity after start, below the number in use, that is not free and not non-solid (solid 0), whose centre origin + (mins + maxs) × 0.5 is within rad of org (the difference per component in float; length = the square root, in double, of the float sum of squares, rounded to float; skipped when the length > rad); 0 if none. (Unlike builtin 22 it walks entity numbers, not the area tree) | 14 |
| 18 | checkclient | → n | as builtin 17, returning a number (0 for none) | 1 |
| 30 | makestatic | n | as builtin 69 (alpha and colour from the extension values) | 1 |
| 31 | setspawnparms | n | as builtin 78 | 1 |
| 87 | makevectors | p angles | as builtin 1: writes the globals `v_forward`, `v_right`, `v_up` | 54 |
| 93 | visibleto | n viewer, n first, i len, buf | (checked: buf, len) zero len bytes of buf; then for each entity first ≤ e < min(first + len, number in use): skip it if free, not linked into the world, or a client slot whose client is not spawned; set buf[e − first] = 1 if any of its leaves is in the fat visibility set of the viewer's eye (origin + view_ofs) | 1 |

### Movement and traces

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 17 | traceline | v start, v end, i nomonsters, n ignore | as builtin 16, including the lag-compensated form when the server's lag compensation setting is 2 (not when `ignore` is a bot client); writes the `trace_*` globals, `trace_ent` as an entity value (world when nothing) | 1 |
| 58 | tracecapsule | v start, v end, i nomonsters, n ignore, v mins, v maxs | as builtin 90 (a box trace; no lag compensation) | 1 |
| 27 | pointcontents | v → i | as builtin 41, the contents code as an int | 21 |
| 24 | walkmove | n, f yaw, f dist → i | as builtin 32 but on entity n instead of `self`: 0 unless on ground, flying or swimming; else the step move with relinking (touches may run: re-entrant); `self` restored after; 1 moved, 0 not | 1 |
| 25 | droptofloor | n → i | as builtin 34 on entity n | 1 |
| 26 | checkbottom | n → i | as builtin 40 | 1 |
| 92 | movetogoal | f dist | as builtin 67 (on `self`) | 1 |
| 29 | aim | | returns 0 and does nothing | 0 |

### Sound, precache and light

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 6 | precache_sound | s | as builtin 19 (only while loading; "Bad string" for an empty name or one starting with a character ≤ space; overflow an error); returns nothing | 203 |
| 7 | precache_model | s | as builtin 20 | 90 |
| 89 | precache_vwep_model | s → i | as builtin 532: returns the list index | 11 |
| 16 | sound | n, i channel, s sample, f volume, f attenuation | as builtin 8 (volume × 255 truncated) | 1 |
| 15 | ambientsound | v pos, s sample, f volume, f attenuation | as builtin 74 | 14 |
| 8 | lightstyle | i style, s pattern | as builtin 35 (the style is not range-checked by the reference: **Ours** 0..63 else an error) | 42 |

### Printing and client text

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 12 | bprint | i level, s, i flags | as builtin 23 with flags: 1 not into demos, 2 not to clients (demos may still get it), 4 into the QTV stream only (not the demo file), 8 not to the server console | 2 |
| 13 | sprint | n, i level, s, i flags | as builtin 24; flag 1 = not into demos; a client not yet connected gets nothing; a non-client prints a console note | 2 |
| 14 | centerprint | n, s | as builtin 73 (a non-client prints a console note) | 1 |
| 19 | stuffcmd | n, s, i flags | as builtin 21 with flags: 1 = not into demos; 2 = into demos only (only text containing a newline is written to the demo stream, nothing to the client, and n is not checked) | 2 |
| 36 | disableupdates | n, f seconds | the client (1..32, else a console note) gets no entity updates until real time + seconds | 1 |
| 33 | logfrag | n killer, n killed | as builtin 79 | 1 |

### Messages

`dest`: 0 the unreliable broadcast, 1 reliably to the client in the global `msg_entity` (1..32,
else the error "WriteDest: not a client"; also copied to that client's demo stream), 2 the
reliable broadcast, 3 the signon (only while loading, else an error), 4 the multicast buffer;
anything else is the error "bad destination". As builtins 52–59, except that the value types
are the C ones:

| # | Name | Args | Writes | Calls |
| --- | --- | --- | --- | --- |
| 37 | writebyte | i dest, i value | the low 8 bits (an int, not a float) | 1 |
| 38 | writechar | i dest, i value | the low 8 bits | 0 |
| 39 | writeshort | i dest, i value | the low 16 bits | 1 |
| 40 | writelong | i dest, i value | 32 bits | 1 |
| 41 | writeangle | i dest, f value | an angle as the protocol's angle | 1 |
| 42 | writecoord | i dest, f value | a coordinate | 1 |
| 43 | writestring | i dest, s | the string and its NUL; a string longer than an overflow-allowed buffer clears the buffer and marks it overflowed instead of failing | 1 |
| 44 | writeentity | i dest, i n | n as a 16-bit value (the entity **number**, not an entity value) | 1 |
| 35 | multicast | v where, i to | as builtin 82 | 20 |
| 45 | flushsignon | | if the signon buffer has less than 512 bytes free, close it and start the next signon buffer (too many buffers is a fatal error) | 1 |

### Cvars, the console and commands

The console's words ("argv") are the console tokenizer's (`configs.md`), shared with the server:
during exports 5, 7, 14 and 18 they are the command being handled (word 0 the command, e.g.
`setinfo key value`, `say …`, `mod …`, or the client's command); system call 84 replaces them.

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 21 | cvar | s name → f | the setting's value as a number (its text read as a number); 0 if it does not exist (no special names, unlike builtin 45) | 3 |
| 67 | cvar_string | s name, buf, size | (checked) the setting's text, truncated to size − 1; empty if it does not exist | 6 |
| 22 | cvar_set | s name, s value | set it; a setting that does not exist is not created (a developer message) | 6 |
| 66 | cvar_set_float | s name, f value | the same with the value printed as `%.8g` (up to 8 significant digits, shortest form; **Ours**: `core/num_text` reproducing that format) | 20 |
| 20 | localcmd | s | append to the server's command buffer | 1 |
| 74 | executecmd | | run the server's command buffer now; `self` and `other` are saved and restored around it | 14 |
| 76 | readcmd | s, buf, size | (checked: buf, size) run the pending buffer; then append s and run it with all console output captured, and copy the capture into buf (truncated). The capture keeps only the **last** stretch: whenever the next print would take it past 1340 bytes the captured text so far is dropped. An outer redirect is suspended and resumed | 19 |
| 77 | redirectcmd | e* client, s | run s with its console output sent to that client as high-level prints (in pieces of at most 1340 bytes, at most 128 pieces); when output is already redirected, just run it. A non-client is an error | 0 |
| 56 | argc | → i | the number of words | 80 |
| 57 | argv | i k, buf, size | (checked) word k (empty when out of range), truncated | 136 |
| 83 | args | buf, size | (checked) the arguments line (the text from word 1) | 2 |
| 84 | tokenize | s | split s into the console's words (this changes what the server's own code sees as the current command) | 6 |
| 68 | map_extension | s name, i number → i | bind an extension call to a number: number − 256 must be < 256, else −2; a null name gives −2 for a number below 94 and −1 otherwise; a name the server provides binds it and returns the number; an unknown name returns −1. Afterwards a system call with that number runs the extension. A call ≥ 94 that is not bound is the fatal error "Bad game system trap: N" | 1 |
| 90 | setpause | i | pause (1) or unpause (0) if the server is not already in that state | 2 |
| 32 | changelevel | s map, s entity file | if `maps/<map>.bsp` does not exist, print "Can't find …" and do nothing; only the first call per map counts; queue `map <map>` or, with a non-empty entity file, `map <map> <entfile>` (the map command then loads that `.ent`) | 2 |

### Info strings and clients

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 34 | infokey | n, s key, buf, size | (checked) copy a value, truncated to size − 1 (a too-small buffer only prints a developer note). See below | 4 |
| 91 | setuserinfo | n, s key, s value, i flags | a key or value containing `&c` or `&r` is ignored. A non-client prints a note. A bot goes to 80. Otherwise: tokenize a three-word command (word 0 a fixed name the module ignores, **Ours** `setinfo`; word 1 the key, word 2 the value, each cut to 64 bytes) into the console's words, set `self` = the client and `time`, call **userinfo changed**(0) (non-zero: stop); set the key (as a star key when flags bit 0); re-read the client's name and settings from its userinfo; call **userinfo changed**(1); if the key is one of the **short-info keys** (`name`, `team`, `skin`, `topcolor`, `bottomcolor`, `chat`, `gender`, `*auth`, `*flag`, `*skill`: the userinfo every client is told about) set it there too and broadcast it to every client (`setinfo` message) | 1 |

**infokey** answers, for n = 0 (the world):
- a key starting with `\`: a support probe. Answer `yes` for `date_str`, `ip`, `realip`,
  `download`, `ping`, `*userid`, `login`, `*VIP`, `*state`, `netname`, `mapname`, `modelname`,
  `version`, `servername` (each after the backslash), else `no`;
- `date_str`: the local date as `Mon Jan 02, 15:04:05 2006` (that layout);
- `mapname`: the map's name (`dm4`); `modelname`: its path (`maps/dm4.bsp`);
- `version`: the server's full version text; `servername`: the server's name;
- otherwise the server info's value, or the local info's when that is empty.

For 1 ≤ n ≤ 32 (that client): `ip` and `realip` its addresses without port; `download` its
download percentage, −1 when none; `ping` its ping as an integer; `*userid`; `login` its account
name; `*VIP` its VIP level as an integer; `netname` its name; `*state` one of `free`, `zombie`,
`preconnected`, `connected`, `spawned`; otherwise its userinfo value. Any other n: empty.

KTX takes the map name and the world model this way on its first start frame, and each
client's name at connect, because module mode writes no strings (§ 3).

### Files

At most 8 open at once; handles are 1..8; all are closed at each map start.

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 59 | fs_open | s name, p handle, i mode → i | writes 0 to *handle first. A name that is empty, starts with `/` or `\`, has `:` as its second character or contains `..` is refused. Modes: 0 read binary, 1 read text, 2 write binary, 3 write text, 4 append binary, 5 append text; anything else refused. Reading looks the name up through the game's search path and returns its **length**; writing creates or truncates, appending opens at the end, both in the game's own directory, and return the current position. Success writes the handle; failure (none free, refused, not found) returns −1 | 8 |
| 60 | fs_close | i handle | close it; an invalid handle does nothing | 6 |
| 61 | fs_read | buf, i count, i handle → i | (checked: buf, count) bytes read; 0 for an invalid handle | 2 |
| 62 | fs_write | buf, i count, i handle → i | (checked) bytes written; 0 for an invalid handle | 5 |
| 63 | fs_seek | i handle, i offset, i origin → i | origin 0 from the current position, 1 from the end, 2 from the start; 0 on success (invalid handle or origin: 0) | 0 |
| 64 | fs_tell | i handle → i | the position; 0 for an invalid handle | 0 |
| 65 | fs_filelist | s path, s pattern, buf, i size, i flags → i | (checked: buf, size) a path containing `..` or `::` returns 0. For each directory of the search path (only the game's own with flags bit 0), list the directory `<dir>/<path>` on disk (not inside packs): every entry except `.` and `..`, files and directories alike, whose name matches the **pattern**, a case-insensitive regular expression found anywhere in the name (`.*` = everything). Each name: with flags bit 1 prefixed by `<dir>/` (a leading `./` dropped); without flags bit 2 its extension removed. At most 4096 names; sort them by byte order; write each non-empty one that differs from the previous, NUL-terminated, back to back, while it fits; return how many were written. KTX's patterns are `\.bsp$` (under `maps`) and `\.ent$`. **Ours**: support at least `.*` and the form `\.<ext>$`, a refusal naming any other pattern | 2 |

### Strings, memory and maths (the module's C library)

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 46 | memset | dst, i byte, i n → dst | (checked: dst, n) fill | 46 |
| 47 | memcpy | dst, src, i n → dst | (checked: both, n) copy. **Ours**: as a move (overlap allowed) | 7 |
| 48 | strncpy | dst, src, i n → dst | (checked: both against n) the C rule: copy up to n bytes, pad with NULs to n, no terminator when src has n or more | 1 |
| 85 | strlcpy | dst, src, i size → i | (checked: dst, size) copy at most size − 1 bytes and terminate (when size > 0); returns the length of src | 75 |
| 86 | strlcat | dst, src, i size → i | (checked: dst, size) append so the total stays below size, terminated; returns the length it tried to make: the initial length of dst (at most size) plus the length of src | 322 |
| 69 | strcmp | s a, s b → i | byte compare: negative, 0 or positive | 45 |
| 70 | strncmp | s a, s b, i n → i | the same over at most n bytes | 9 |
| 71 | stricmp | s a, s b → i | ASCII case-insensitive | 0 |
| 72 | strnicmp | s a, s b, i n → i | the same over at most n | 0 |
| 82 | strftime | buf, i size, s format, i offset → i | (checked) the local time now + offset seconds, formatted with the C `strftime` conversions into at most size − 1 bytes; returns the length; 0 (and an empty buf) when it does not fit or size ≤ 0. **Ours**: the platform's wall clock (presentation only, never simulation) and our own formatter for the conversions KTX uses (`%Y %m %d %H %M %S %a %b %y %j %Z` and the like) | 12 |
| 49 | sin | f → f | the float argument widened to double, the function in double, rounded to float. **Ours**: `core/libm` | 7 |
| 50 | cos | f → f | the same | 6 |
| 51 | atan2 | f y, f x → f | the same | 3 |
| 52 | sqrt | f → f | the same | 7 |
| 53 | floor | f → f | the same | 14 |
| 54 | ceil | f → f | the same | 14 |
| 55 | acos | f → f | the same | 1 |

### Bots

| # | Name | Args → result | Does | Calls |
| --- | --- | --- | --- | --- |
| 78 | add_bot | s name, i bottom colour, i top colour, s skin → n | 0 when the players are at the limit or no slot is free. Otherwise take the first free client slot, clear its entity, give it the userinfo `\name\<name>\topcolor\<t>\bottomcolor\<b>\emodel\6967\pmodel\13845\skin\<skin>\*bot\1`, mark it fully in the game, a bot, with a new user id and the connection time; the `gravity` field 1, the `maxspeed` field the server's maximum speed, the `isBot` field (an int) 1, `colormap` = its number; copy the short info; then with `self` = it and `time`: **client connect**(0) and **put client in server**(0) (re-entrant); returns its number | 1 |
| 79 | remove_bot | n | a non-bot or non-client prints a note. Otherwise **client disconnect**(0) with `self` = it, then zero its frags and name, free the slot, clear its info and announce the change | 1 |
| 80 | set_bot_userinfo | n, s key, s value, i flags | `&c`/`&r` ignored; non-bots print a note. Set the key (star when flags bit 0), re-read the name and settings; if the key starts with `_` or is a short-info key, set it in the short info and broadcast it. No userinfo-changed export is called | 12 |
| 81 | set_bot_cmd | n, i msec, f pitch, f yaw, f roll, i forward, i side, i up, i buttons, i impulse | store the bot's command for its next bot frame (§ 3). If the bot's `fixangle` is set, the command's angles become the entity's `angles` with the pitch multiplied by −3, and `fixangle` is cleared | 2 |

### Extension calls (bound by number through 68)

KTX asks for these thirteen names with these numbers at init, and then calls a number only if
its binding succeeded:

| # | Name KTX asks for | The reference | **Ours** | Calls |
| --- | --- | --- | --- | --- |
| 256 | SetExtField | not provided (−1) | not provided | 3 |
| 257 | GetExtField | not provided | not provided | 1 |
| 258 | ChangeLevelHub | not provided | not provided | 0 |
| 259 | URI_Query | not provided | not provided | 0 |
| 260–262 | particleeffectnum, trailparticles, pointparticles | not provided | not provided | 0 |
| 263 | clientstat | provided only with client-side QC | not provided | 0 |
| 264 | pointerstat | the same | not provided | 0 |
| 265 | MapExtFieldPtr | provided | **provided** | 5 |
| 266 | SetExtFieldPtr | provided | **provided** | 4 |
| 267 | GetExtFieldPtr | provided | **provided** | 1 |
| 268 | setsendneeded | provided only with client-side QC | not provided | 1 |

- **MapExtFieldPtr**(s name) → i token: a non-zero opaque token for a per-entity extension
  value, 0 for an unknown name. The reference knows `alpha` (one float: 0 or 1 opaque, between
  translucent), `colormod` (three floats, 1 = unchanged), and with client-side QC `sendflags`,
  `SendEntity`, `pvsflags`. Its token is the value's byte position in its per-entity extension
  record OR a random cookie in the high 16 bits, so a module cannot hard-code positions.
  **Ours**: tokens for `alpha` and `colormod` only (0 for the rest, so KTX never believes it can
  send entities); any non-zero encoding the next two calls recognise.
- **SetExtFieldPtr**(e* entity, i token, src, i size) → i: copy size bytes from module memory
  into that entity's extension value; 1 on success, 0 (with a console note "Corrupt field
  reference" or "out of bounds") for a bad token or a size past the value. **Ours**: size must be
  4 for alpha, 4 or 12 for colormod (at most the value's size).
- **GetExtFieldPtr**(e* entity, i token, dst, i size) → i: the reverse copy.
- These are the same per-entity alpha and colour values the entity text's `alpha` and
  `colormod` keys set (`builtins.md` § Map entity keys); clients that support them receive them,
  makestatic carries them, and freeing or clearing an entity resets them to zero.

---

## 5. Shared memory: the game data block, globals, entities and strings

Entities and globals live **inside the module's memory**; the server reads and writes them there
directly, at fixed offsets, every frame.

### The game data block

Init returns the VM address of six i32:

| # | Field | KTX's value (measured) |
| --- | --- | --- |
| 0 | ents: VM address of entity 0's record | 2 496 648 |
| 1 | entity size in bytes (the record stride) | 7 892 |
| 2 | globals: VM address of the global block | 2 496 224 |
| 3 | fields: VM address of the extension field table | 8 |
| 4 | API version | 16 |
| 5 | maximum entities | 2048 |

Entity n's record is at ents + n × size. The server binds slot n to that address for every
n below the entity limit. (The block itself is at VM address 164 in the measured file.)

### The globals

The global block has the same word layout as the QC globals (`progs.md` § The engine-side
interface): words 0..27 are padding, then `self` at word 28 (byte 112), `other`, `world`,
`time`, `frametime`, `newmis`, `force_retouch`, `mapname`, `serverflags`, … through
`SetChangeParms` at word 89; 90 words of engine globals followed by nothing the server uses
(the block is 424 bytes from the measured addresses: 106 words). Byte offset = word × 4.

The server writes and reads the same globals at the same moments as on the QC path (`self`,
`other`, `time`, `frametime`; the `trace_*` results; `v_forward`/`v_right`/`v_up`; `parm1..16`;
`serverflags`; reads `force_retouch` and decrements it, reads `newmis` and `msg_entity`). It never
writes `mapname` and never calls the function globals (`main`, `StartFrame`, …): exports replace
them. Entity-valued globals (`self`, `other`, `world`, `newmis`, `trace_ent`, `msg_entity`) hold
**entity values**: n × entity size, the world 0, exactly as on the QC path but with the
module's stride.

### The entity record

The record begins with the engine fields in the QC order and sizes (`progs.md`, the field
list), 102 words, 408 bytes; the module's own data follows. The byte offsets:

modelindex 0, absmin 4, absmax 16, ltime 28, lastruntime 32, movetype 36, solid 40, origin 44,
oldorigin 56, velocity 68, angles 80, avelocity 92, classname 104, model 108, frame 112, skin 116,
effects 120, mins 124, maxs 136, size 148, touch 160, use 164, think 168, blocked 172,
nextthink 176, groundentity 180, health 184, frags 188, weapon 192, weaponmodel 196,
weaponframe 200, currentammo 204, ammo_shells 208, ammo_nails 212, ammo_rockets 216,
ammo_cells 220, items 224, takedamage 228, chain 232, deadflag 236, view_ofs 240, button0 252,
button1 256, button2 260, impulse 264, fixangle 268, v_angle 272, netname 284, enemy 288,
flags 292, colormap 296, team 300, max_health 304, teleport_time 308, armortype 312,
armorvalue 316, waterlevel 320, watertype 324, ideal_yaw 328, yaw_speed 332, aiment 336,
goalentity 340, spawnflags 344, target 348, targetname 352, dmg_take 356, dmg_save 360,
dmg_inflictor 364, owner 368, movedir 372, message 384, sounds 388, noise 392, noise1 396,
noise2 400, noise3 404.

- **Floats and vectors** are IEEE singles, read and written by the server exactly as the QC
  path does (`gameloop.md` names which ones each step uses: origin, angles, velocity, mins,
  maxs, solid, movetype, flags, nextthink, ltime, modelindex, frame, skin, effects, colormap,
  …).
- **Entity fields** (`groundentity`, `chain`, `enemy`, `aiment`, `goalentity`, `dmg_inflictor`,
  `owner`) hold entity values (n × entity size), as the globals.
- **Function fields** (`touch`, `use`, `think`, `blocked`) are only tested for zero by the
  server (§ 3); the module keeps its real function pointers in its own part of the record.
- **String fields** (`classname`, `model`, `weaponmodel`, `netname`, `target`, `targetname`,
  `message`, `noise`, `noise1..3`) are described next.
- The server's own per-entity state (free flag, free time, links, leaves, baseline, the
  extension values) is outside module memory.

### Strings

A string the server reads from the module is always a VM address of NUL-terminated bytes; 0
means the empty string. The `mapname` global is never used.

**String fields** have two encodings, chosen once per map at init (§ 3 step 4):

- **Reference mode** (KTX: always): the field's word holds a **byte offset from ents** to a word
  in the module's memory (inside some entity record: the module points it at its own pointer
  member), and that word holds the VM address of the string. KTX sets entity n's reference
  for each string field to n × size + the byte offset of its own matching pointer member, and
  initialises the pointer to 0; it does this in **clear edict** and after spawn, and the
  function fields' words get the same treatment (non-zero offsets).
- **Direct mode** (a module that does not ask for references): the field's word holds the VM
  address itself.

**Reading a string field** (the server does: `model` to decide whether an entity is sent and for
makestatic, `weaponmodel` for the weapon status, the world's `message` for the level name in the
connection data, `classname` to find `info_player_start` and in two diagnostics):

1. Let w be the field's word. w ≤ 0 → "".
2. Reference mode: w must satisfy 0 < w < entity size × entity limit − 4, else the reference
   dereferences nothing and crashes (**Ours**: ""); read the word at ents + w as the new w;
   w ≤ 0 → "" (the reference does not test this second zero and can crash on it; **Ours**: "").
3. The string at VM address w (**Ours**: checked to end inside memory, else "" and a warning).

**Writing a string field** happens only in setmodel (11): in reference mode the VM address of
the name is written into the word the `model` reference names (same range rule); in direct mode
into the `model` word. No other server code writes module strings.

### The extension field table

An array of 12-byte records {name: VM address of a string, offset: i32 **byte** offset inside
the entity record, type: i32}, ended by a record whose name is 0. Types: 0 int, 1 float,
2 string, 3 vector, 4 angle, 5 ignore. The server looks fields up by name, **case-insensitive**,
first match wins, after init; an absent name gives offset 0 = feature off. The measured table:

| Name | Offset | Type | The server |
| --- | --- | --- | --- |
| vw_index | 472 | float | as `builtins.md` § The extension fields |
| movement | 540 | vector | as there |
| maxspeed | 476 | float | as there; also set at a client's set-up and for a new bot |
| gravity | 480 | float | as there; set to 1 at set-up and for a bot |
| isBot | 484 | int | set to 1 (the int) for a bot it adds |
| brokenankle | 488 | float | not read |
| items2 | 2884 | float | as there |
| hideentity | 3656 | int | an entity value; the entity it names is not sent to this client |
| trackent | 3708 | int | a client **number** 1..32 (read raw) |
| hideplayers | 3660 | int | non-zero: other players are not sent to this client |
| mod_admin | 956 | int | bit 1 (value 2) means a real admin: required by the server's client-side ban commands |
| teleported | 7792 | int | as there (non-zero, then cleared to 0) |

`visclients` is not in KTX's table, so that feature is off. Note the int fields: the server reads
the raw word, so the module's int 1 and the QC path's float 1.0 both read as "set".

### Clearing and freeing

- **Clear** (allocation, a client's set-up, a bot): zero the whole record (entity-size bytes) and
  the extension values, mark it in use, forget its last-run time; then, in reference mode, call
  **clear edict** with `self` = it (restored after).
- **Free**: as system call 5.
- The world (entity 0) and the client slots are bound at map start and cleared as above.

---

## 6. What is new compared with the QC path

What `plugins/progs` and the server's QC path already have, and what a module needs instead:

| Topic | QC path (`progs.md`, `builtins.md`) | Module path |
| --- | --- | --- |
| The machine | 66 three-operand opcodes on a global block of floats | 60 stack-machine opcodes on bytes, ints and floats (§ 2): an operand stack, a program stack in memory, sign extension, unsigned maths, block copies, computed jumps |
| Memory | globals and an engine-owned entity array of `entityfields` words | one flat module memory (a power of two, 32 MiB for KTX) that **contains** the entities and globals; the server reads and writes them in place |
| Entity stride | `entityfields` × 4 | the block's entity size (7 892 for KTX) |
| Entity values | number × stride | the same rule, the module's stride; system calls mostly take **entity numbers**, and some take or return **entity addresses** (find, findradius, nextclient, redirectcmd, the extension calls) |
| Strings | offsets into the string table or engine handles, temporary rings | VM addresses; the module owns every string; string fields through **references** (§ 5); results are written into **buffers the module passes**, never returned as handles; no string ring |
| Calls in | the engine runs a named function per hook, found by global or by name | one entry point with a command number (§ 3); think/touch/blocked dispatch is the module's own |
| Calls out | builtins by number, arguments in parameter globals as floats | system calls by number (−address − 1), arguments on the program stack as raw words (ints, addresses, float bits) |
| Spawning the map | the engine parses the entity text, fills fields by name, applies the skill and deathmatch inhibit flags and calls spawn functions | the module does all of it, reading tokens through system call 3 during export 1 |
| Strings the engine writes | the world's model, netname, targetname, the mapname global, each client's netname | none: the module asks with infokey (`mapname`, `modelname`, the client's `name`) |
| Hooks found by name | SpectatorConnect, UserCmd, GE_ClientCommand, ConsoleCmd, UserInfo_Changed, ChatMessage, localinfoChanged, GE_ShouldPause, GE_PausedTic, timeofday | none; the exports 3–19. Unknown console commands are not offered; `mod …` is |
| Assigning to the world | an error while the map runs | not detectable (the module writes its memory directly) |
| Extension discovery | `checkextension` names | api_version (0) and map_extension (68) by name and number |
| C library | none | memset, memcpy, strncpy, strlcpy, strlcat, the compares, the maths, strftime, files, file lists, console capture (readcmd), bots |
| Re-entry | a builtin may run QC hooks | system calls re-enter the module (spawn, walkmove, add_bot, setuserinfo, executecmd, readcmd) |
| Run-away guard | 100 000 statements | none in the reference |
| Fresh state | the globals reload from the file each map | the whole memory reloads from the file each map |

Behaviour that differs from the builtin of the same name (details in § 4): findradius walks
entity numbers and returns one entity per call; find and findradius return 0, not the world;
walkmove and droptofloor take the entity instead of using `self`; the write calls take ints;
writeentity takes a number; bprint, sprint and stuffcmd take flags; changelevel checks the map
exists and takes an entity file; cvar has no special names; infokey knows more keys; precache
calls return nothing (except the visible-weapon one).

**New server-side pieces** a building session needs: the `.qvm` loader and verifier; the
interpreter (an optional translator later, `perf` skill); the system-call table; an entity store
that can live in module memory (base address + stride) behind the same accessors the game loop
uses for QC entities; string-field reading through references; the export calls at the QC
hooks' places; per-entity alpha and colour values (if not already there for the entity keys);
the bot client (add, remove, userinfo, stored commands, bot frames); console capture for
readcmd; the mod's file area and file listing; the `sv_pr2references` cvar (registered, reset to
0 before each init); `*progs` from the file's CRC.

---

## 7. Measured facts about `qwprogs.qvm`

Measured by decoding the file (a throwaway tool in the session's scratch space, not committed).

**The header**: magic `0x12721445` (version 2); 405 728 instructions; code at offset 36,
1 222 356 bytes (fully consumed by the instructions); data at offset 1 222 392, 40 952 bytes;
lit 283 552 bytes; bss 22 385 960 bytes; jump targets 12 120 bytes (3 030 targets); the file ends
exactly there (1 559 016 bytes).

**Memory**: exact length 22 710 464 bytes; + 32 768 = 22 743 232; memory size 33 554 432
(2²⁵, 32 MiB), data mask `0x01FFFFFF`; the stack starts at 33 554 432, bottom limit
33 456 128. Of the bss, 2048 × 7 892 = 16 162 816 bytes are the entity records.

**Code shape**: 3 865 functions (ENTERs; each ends in PUSH, LEAVE); 5 314 LEAVEs; the first
instruction is ENTER 108 (the entry function); frame sizes 8..4 244 bytes; ARG offsets 8..108;
LOCAL operands up to 4 252; 16 485 direct calls, 56 indirect (through function pointers), 37
computed jumps (switch tables); no call to instruction 0; no shift by a constant outside 0..31;
no division by a constant 0; 129 block copies, all multiples of 4 bytes, the largest 2 048.

**Opcode counts** (every opcode below appears at least once except where 0):

| Op | Count | Op | Count | Op | Count | Op | Count |
| --- | --- | --- | --- | --- | --- | --- | --- |
| UNDEF | 0 | EQF | 1 023 | SEX8 | 186 | BXOR | 22 |
| IGNORE | 0 | NEF | 921 | SEX16 | 13 | BCOM | 8 |
| BREAK | 0 | LTF | 199 | NEGI | 13 | LSH | 840 |
| ENTER | 3 865 | LEF | 421 | ADD | 23 523 | RSHI | 7 |
| LEAVE | 5 314 | GTF | 114 | SUB | 425 | RSHU | 6 |
| CALL | 17 984 | GEF | 496 | DIVI | 50 | NEGF | 54 |
| PUSH | 3 865 | LOAD1 | 275 | DIVU | 0 | ADDF | 3 322 |
| POP | 9 096 | LOAD2 | 12 | MODI | 62 | SUBF | 901 |
| CONST | 109 430 | LOAD4 | 70 558 | MODU | 1 | DIVF | 159 |
| LOCAL | 64 922 | STORE1 | 257 | MULI | 727 | MULF | 1 026 |
| JUMP | 6 319 | STORE2 | 6 | MULU | 20 | CVIF | 951 |
| EQ, NE | 4 610, 3 204 | STORE4 | 33 540 | BAND | 826 | CVFI | 1 553 |
| LTI, LEI, GTI, GEI | 581, 279, 164, 176 | ARG | 33 004 | BOR | 211 | | |
| LTU, LEU, GTU, GEU | 34, 13, 2, 9 | BLOCK_COPY | 129 | | | | |

**System calls used**: every number 0..93 has at least one call site except 29 (aim), 38
(writechar), 63 (fs_seek), 64 (fs_tell), 71 (stricmp), 72 (strnicmp) and 77 (redirectcmd);
of the extensions, 256, 257, 265, 266, 267 and 268 have call sites (each guarded by its binding);
258–264 have none. The per-call counts are the **Calls** column of § 4. The heaviest: strlcat
322, precache_sound 203, argv 136, precache_model 90, argc 80, strlcpy 75, makevectors 54,
memset 46, strcmp 45, lightstyle 42.

**The game data block** (VM address 164): ents 2 496 648, entity size 7 892, globals 2 496 224
(424 bytes before ents), field table at 8 (the twelve fields of § 5), API version 16, maximum
entities 2048.

**Strings in the file** worth knowing: the extension names of § 4 sit in lit near VM address
41 000 (`setsendneeded`, `MapExtFieldPtr`, `pointerstat`, `clientstat`, `pointparticles`,
`trailparticles`, `particleeffectnum`, `URI_Query`, `ChangeLevelHub`, `SetExtField`);
`sv_pr2references` (set to 1 at init), `qwm_name`, `qwm_version`, the version `1.46-dev`, the
init refusal "Mod requires API_VERSION %d or higher, server have %d", and the server-family probe
on `*version` containing "FTE" (ours must not contain it, or KTX takes the other server's
paths).

**Server info**: `*progs` = 27 (the CRC of § 1).

**Not measured here** (needs a run; the building session's labs): the order and count of system
calls during a map start and a frame, the time per frame, and the module's memory written per
frame. A lab that runs the module on our server and logs every system call and export (number,
arguments, result) gives all three and is the first thing worth building.

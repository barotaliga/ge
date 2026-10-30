# The QC language, specified

The language a mod's game logic is written in, in our words (README.md, the rules), for our
compiler (`plugins/progs/qc_compile.hpp`, M18d). Sources: the language's public reference manual (the types, the
declarations, the statements, the frame macros, the project file) and the bytecode note
(`progs.md`), which fixes what the compiled program must look like. No compiler's source was
read for this note; where the manual leaves a choice open, this note makes it and says **Ours**.

## Files

- **The project file** (`progs.src`): the first word is the output file's name, relative to the
  project file's directory; every later word is a source file, compiled in that order as one
  unit. `//` comments are allowed in it.
- **A source file** is plain text. `//` comments run to the end of the line, `/* */` comments
  do not nest. Identifiers are a letter or `_`, then letters, digits and `_`; they are case
  sensitive.

## Types

| Type | Words | Value |
| --- | --- | --- |
| `void` | 0 | nothing (a function's result only) |
| `float` | 1 | a 32-bit IEEE float, the only number |
| `vector` | 3 | three floats |
| `string` | 1 | an offset into the string table (0 is the empty string) |
| `entity` | 1 | an entity reference (the world is 0) |
| function | 1 | a function index; written `float(float a, vector b)` before the name |
| field | 1 | written `.float`, `.vector`, `.void()`…: an offset inside every entity |

A function type lists its parameters in parentheses: `void(entity e, float f)`; up to 8
parameters, each `float`, `vector`, `string`, `entity`, a function or a field type; a final
`...` makes it variadic (a builtin only: extra arguments of any one-word or vector type, up to
8 in all).

## Declarations (file scope)

```
float  health_max = 100, armour_max;      // globals; one initialised is a CONSTANT
vector up = '0 0 1';                      // a vector literal: three numbers in single quotes
string greeting = "hello";
.float frags;                              // a field every entity has
.void() think;                             // a field holding a function
void(vector v) makevectors = #1;           // a builtin, by number
float(float a, float b) add;               // a prototype (a forward declaration)
float(float a, float b) add = { return a + b; };   // a definition
void() wait_frame = [$stand1, wait_next] { ... };  // a frame function (below)
```

- **The prototype form** puts the type, with its parameter list, before the name. **Ours**:
  the C form `float add(float a, float b) { … }` and `void add(float a, float b);` is also
  accepted and means the same.
- A declaration of a name already declared must have the same type; a function may be defined
  once.
- **Constants**: a global declared with an initialiser is a constant: assigning to it is an
  error. `float`, `vector`, `string`, function and entity-less constants only.
- **Vector members**: declaring a vector `v` (a global, a local or a field) also declares
  `v_x`, `v_y`, `v_z` as floats at its three words (fields `.float v_x` at the field's three
  offsets).
- **The engine's own globals and fields come first.** The program must declare the engine-side
  globals and fields in exactly the order of `progs.md` § The engine-side interface, before any
  of its own. **Ours**: the compiler checks that layout (names, types, offsets) and writes the
  layout's crc (54730) only when it matches; a program that does not match gets crc 0 and a
  warning, and an engine refuses it. The declarations `void end_sys_globals;` and
  `void end_sys_fields;` mark the ends of the two lists and occupy nothing.

## Functions

- A body is `{ statements }`. Parameters are named in the type: `float(float a, float b)`.
- **Locals**: `local float x;`, `local vector dir = '0 0 1';` (an initialiser is an ordinary
  assignment, run each time). **Ours**: `local` may be omitted (`float x;` inside a body).
  Locals live until the function ends (no block scope in the classic language). **Ours**: a
  name declared twice in one function is an error.
- **Frame functions**: `void() run3 = [$run3, run4] { body };` sets, before the body runs, the
  entity in `self`'s `frame` to the first value, its `think` to the second (a function name)
  and its `nextthink` to `time + 0.1` (the STATE opcode). The first may be any float
  expression of a constant or a frame macro.
- **Builtins**: `= #N;` binds the name to builtin number N (a function record with first
  statement −N).
- A function that is not void and reaches its end returns 0 (the compiler emits DONE).

## Frame macros

A line starting with `$` is a model-description line. `$frame a b c` gives each of `a`, `b`, `c`
the next frame number (counting from 0 in the file, or from the last `$framevalue N`); `$name`
anywhere in an expression is that number as a float constant. `$cd`, `$origin`, `$base`,
`$skin`, `$flags`, `$scale` and `$modelname` lines are read and ignored.

## Statements

`expression;` · `{ … }` · `if (e) s` · `if (e) s else s` · `while (e) s` · `do s while (e);` ·
`return;` · `return e;` · `local` declarations. **Ours** (common in today's mods): `for (init;
cond; step) s`, `break;`, `continue;`, and `;` alone.

**Truth** in a condition: a float is true when its word is not zero (the IF opcode tests the
raw word, so −0 is true there although `!−0` is 1); a vector when any component is not zero; a
string, an entity or a function when its raw word is not 0 (so the null string, the world and the
null function are false; `!s` and `s == ""` compare by content, so an empty string made
at run time is false to `!` but true to `if`).

## Expressions

Precedence, from loosest to tightest (**Ours**: the C order, as today's mods are written for):

| Level | Operators | Notes |
| --- | --- | --- |
| 1 | `=` `+=` `-=` `*=` `/=` | right to left; the left side is a variable, a vector member or an entity's field |
| 2 | `\|\|` | |
| 3 | `&&` | **both sides are evaluated** (the AND and OR opcodes do not skip) |
| 4 | `\|` | bitwise on whole floats (the value truncated toward zero) |
| 5 | `&` | |
| 6 | `==` `!=` | float, vector, string (by content), entity, function |
| 7 | `<` `>` `<=` `>=` | floats only |
| 8 | `+` `-` | float and vector |
| 9 | `*` `/` | `float*float`, `vector*vector` (the dot product), `float*vector`, `vector*float`; `vector/float` is `vector * (1/float)` |
| 10 | `!` `-` (unary) | `-x` is `0 - x` (a vector: `'0 0 0' - v`) |
| 11 | call `f(a, b)`, field `e.f`, member `v.x` | `e.f` reads entity `e`'s field `f`; `e.f.x` / `e.v_x` a vector field's component |

- **Literals**: decimal numbers (`3`, `0.5`, `.5`, `1e3`), `"strings"` with `\n`, `\"` and `\\`
  escapes (adjacent strings are joined), `'x y z'` vectors, frame macros `$name`.
- **Calls** evaluate every argument, left to right, before the first is put in the parameter
  slots, so a call
  inside an argument does not overwrite another argument; a result is copied out of the return
  slot before the next call. A call with fewer arguments than parameters is an error, except to
  a variadic builtin.
- **Assignments to a field** `e.f = v` take the field's address (ADDRESS) and store through it
  (STOREP). The engine forbids writing the world's fields once the map runs.
- **Types must match**: `float` with `float`, and so on; there is no implicit conversion. A
  function name used as a value is its function value; `world` is an entity global.

## What the compiled program looks like

`progs.md` is the format. The compiler:

1. reserves globals 0–27 (null, return, the eight parameter slots), then lays out every global
   in declaration order (a vector three words), then each function's parameters and locals as
   one block of globals per function (the `locals` range the VM saves on entry), its temporaries
   inside that block too, so a recursive call keeps them;
2. puts each distinct float, vector and string constant in one shared global;
3. gives every named global, field and local a definition record (type, offset, name), fields
   both a field record and a global record holding the field's offset (the value an entity load
   names);
4. writes statement 0 and function 0 as the null slots, then one function record per function
   in the order each was first declared (a prototype takes its slot; builtins too), and the
   strings table with the empty string at 0;
5. ends every function with DONE.

**Ours, determinism**: the output depends on the source text alone (no time, no path but the
names written into the file, no hash order), so a mod built twice is byte for byte the same.

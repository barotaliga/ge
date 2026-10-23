# The console language, specified

A specification in our words of the console language players' configs use, the classic core
and what today's clients added (README.md, the rules). `docs/shooters-edge.md` § Configs lists the
features; this note grows the exact behaviour of each, with a fixture per feature, when M18k
starts. Each section below is a heading to fill, with the facts and where each came from.

## Lexing
Lines, `;` splitting outside quotes, quoting, `//` comments, the line-length limit.

## The command buffer
Insertion versus appending (`exec` inserts, a stuffed command appends), `wait` (one frame),
recursion limits.

## Cvars
Names, values as strings and numbers, flags (archived, server info, user info, cheat, latched,
read-only), `set`, `seta`, `setinfo`, `serverinfo`, `toggle`, `inc`, `dec`, `cycle`, unknown
cvars created on `set` and kept.

## Aliases
Definition, arguments `$1`…`$9` and `$*`, `+`/`-` aliases on a held key, temporary aliases, the
depth limit.

## Expansion
`$cvar` in a command line, escaping a literal `$`, when expansion happens (at execution, not at
definition).

## Conditionals and arithmetic
`if` / `else` over numbers and strings, the comparison operators, the calculation command's
operators (arithmetic, string length, substring, search) and its output into a cvar.

## Binds
Key names, the bind tables by mode (game, console, menu, demo), modifiers, `+` commands on
press and release.

## Team messaging
Macros expanded at send time, location files, message triggers and their matching, the filters.

## Saving
`cfg_save`'s order and contents, `cfg_load`, `autoexec.cfg`, per-mod configs.

## What a server may stuff
The allowed and refused commands, the flags that guard cvars.

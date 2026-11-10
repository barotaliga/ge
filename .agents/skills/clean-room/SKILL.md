---
name: clean-room
description: "Use BEFORE any answer you need comes from source code this tree may not copy: a GPL client, server, mod or tool (the projects porting/shooters-edge/survey.md names), or any third-party code whose licence is not this tree's. Porting a behaviour, verifying that ours matches it, settling a question about it. A READER subagent reads the source and writes a question-answering document in its own words with no code; the IMPLEMENTER (you) builds or verifies from that document alone and never opens, greps or diffs the source. Owns the reader's brief, the answer document's shape, the screen for code and where the facts are kept."
---

# Clean room: a reader answers, the implementer builds

This tree is under a noncommercial licence, so no GPL code may enter it, and nothing that reads
as a translation of GPL code may either (`docs/shooters-edge.md` § Licensing and the clean room).
Copyright protects code, not facts: a format, a protocol, a number, an order of events. The
clean room turns code into facts in a place the code never leaves.

**The rule: the agent that writes or verifies engine code never reads the source.** Not a
file, not a grep hit, not a diff, not a line quoted in a reply. Every answer that needs the
source comes from a **reader**: a separate agent that reads it and hands back a document of
facts in its own words. Only that document crosses to the implementer.

This skill asks for a subagent, so spawning the reader is always allowed. Load the skill of the
subsystem you build beside it. Read the lessons first: `make lessons ARGS=clean-room`.

## When it applies

| Situation | Clean room? |
| --- | --- |
| Porting or matching a behaviour of a GPL client, server, mod, compiler or tool | Yes |
| Verifying that our code does what one of them does ("is our clamp the same?") | Yes |
| A question about one of them that the notes in `porting/` do not answer | Yes |
| Any other third-party source whose licence is not this tree's, or is unknown | Yes |
| A public format description, a protocol write-up, a language reference (prose, not code) | No: read it, cite it |
| Running a program and measuring what it does (a lab, a recording, a packet capture) | No: black-box facts, anyone may take them (`docs/shooters-edge.md` § step 3) |
| Code under this tree's own licence (the `port` skill's sources) | No: the `port` skill |
| A mod's bytecode the VM executes as data | No: it is data |

**First look in the notes.** `porting/shooters-edge/README.md` lists every note; `make find
Q="<words>"` searches them with the code. A note that answers the question is the document:
build from it and spawn no reader. A reader is for the question the notes leave open.

## The two roles

| | Reader | Implementer |
| --- | --- | --- |
| Who | A fresh subagent (`Agent`, `subagent_type: general-purpose`), never a fork | You, the session that writes the code |
| Reads | The source, in its scratch clone; the questions | The questions it wrote, the answer document, the notes, the labs' measurements, this tree |
| Writes | ONE answer document, nothing else | The engine's code, tests, labs |
| Never | Edits the tree beyond its note; puts code in the document; returns anything but the document | Opens, greps, `find`s, diffs or builds-and-reads the source; asks for "the function"; reads the reader's scratch |

A fresh agent, not a fork, so the reader starts from the questions alone and keeps its reading
out of your context. Its final message enters your context, so its final message IS the
document (or its path), nothing more.

## The procedure

1. **Write the questions** in `<scratchpad>/clean-room/<topic>-questions.md`. Ask for
   behaviour, not code: "what happens to the velocity when the player lands on a slope steeper
   than the walkable limit, and in what order are friction and gravity applied?", "which bytes
   follow the message's id, with their types and ranges?", "what default does the server give
   the setting, and what values does it refuse?". Number them. Say what you already know from
   the notes so the reader does not repeat it. For a VERIFICATION, state our behaviour in words
   as claims to confirm or refute ("we clamp the step to 18 units; we apply friction before the
   jump"), never paste our code.
2. **Spawn the reader** with the brief below, filled in. Run it in the background and keep
   working on what does not depend on it.
3. **Screen the document** before you build on it (§ The screen). A document that fails goes
   back to a new reader with the failure named. Do not read past the first code-shaped line.
4. **Keep the lasting facts.** A fact the next session will need goes into the subsystem's note
   under `porting/` (for Shooter's Edge, `porting/shooters-edge/<note>.md`, listed in that
   directory's README), each fact marked `read`, `measured` or `derived` (the lesson on
   provenance: derived facts are the wrong ones, re-ask them). A one-off answer stays in the
   scratchpad. The reader writes the note; you commit it.
5. **Build or verify from the document**, the notes and the measurements. Name the facts you
   relied on in the PR's description by the note they live in. Where the feel must match, hold
   the code to a black-box measurement in a lab, not to the document's prose alone.
6. **A new question is a new round.** Write it down and send it to the reader (`SendMessage`
   to the same reader keeps its context; a new reader is fine too). Never "just check" the
   source yourself, however small the question.

## The reader's brief

Paste this into the `Agent` prompt, with the brackets filled:

```text
You are the READER in a clean room. Licence: [project, licence]. You read [project] at
[release or commit], cloned into [<scratchpad>/clean-room/src/<project>] (clone it there if it
is not, never inside the repository's checkout). Answer the numbered questions in
[<scratchpad>/clean-room/<topic>-questions.md] [and write the lasting facts into
porting/<area>/<note>.md, the only file in the repository you may change].

Write ONE document, [<scratchpad>/clean-room/<topic>-answers.md], that answers each question
by its number in your own words, as facts: numbers with units, byte layouts, orders of events,
defaults and limits, edge cases, the names a player or a mod sees (settings, commands,
messages, builtins, file fields). Mark each fact read, measured or derived. Name only the
project and release you read, never a file, function, line or variable of it.

The document must contain NO code: no source lines, no pseudocode, no function walked through
statement by statement, no comment quoted, no table copied out of the source that the format
does not itself define (derive or measure such values and say how). If a question can only be
answered by quoting code, answer what the code DOES, or say it is unanswerable without quoting.
If the source does not settle a question, say so; do not guess.

Before you finish, run the screen in .agents/skills/clean-room/SKILL.md § The screen over your
document and fix what it finds. Your final message is the document's path and one line per
question saying answered, partly answered or open. Nothing else: no excerpts, no summary of the
source's structure.
```

## The screen

Run it on every document before reading it closely (the reader runs it too):

```sh
grep -nE '^(```|    \S)|[{};]\s*$|\b(if|for|while|switch|return)\s*\(|->|::|==|!=|&&|\|\|' <doc>
```

Each hit is code-shaped. A formula in prose ("speed × friction × frame time") is a fact and
passes; a C expression does not. Also reject, by eye: an identifier a player or a mod never
sees (an internal variable or function name), a step list that mirrors one function's
statements in order, a file or line reference into the source, and anything longer than a
number copied verbatim. A table the format defines (a palette's role, the packed normals) is
fine only with how it was derived or measured.

## Rules

- **One-way glass.** Facts flow from the reader to the implementer, never source. The reader
  may read our tree; the implementer may never read theirs. If you saw source anyway (a hit in a
  broad grep, a pasted snippet), stop, say so to the person in the thread, and do not write the
  part of the subsystem it touched until they decide.
- **No source in the tree, the PR or a message**: not in a commit, a test, a golden, a lab's
  default, a note, a PR description, a reply or an Artifact page. The clone lives in the
  scratchpad and is deleted when the session ends.
- **Running is not reading.** A lab may fetch and build an existing server or client into its
  own cache and measure it; the implementer runs it as a black box and reads only its output.
- **Outside `porting/`, name no project** (AGENTS.md § A clean slate): the facts land in code
  and docs in the engine's own terms.

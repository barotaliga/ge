# Setting bans and user info bans: answers

Read from: Q2PRO, the actionquake fork, default branch as cloned on 2026-11-19 (head commit 6400f7e). For question 8 also: id Software's published Quake II source (its version string reads 3.19; it is the tree id released as the 3.21 source, the closest published source to the 3.20 client) and id Software's published Quake source, its QuakeWorld tree (version 2.40).

Tags: [read] = seen in the source; [derived] = follows from what was read, not run; [measured] = none in this document (nothing was run).

## 1. How the server learns a setting's value

- The server sends the client a reliable stufftext message (a console line the client runs), one line per setting ban, of this shape: the word `cmd`, a space, a two-byte command name made of the byte 0x7F (DEL) followed by the letter `c`, a space, the setting's name, a space, a dollar sign immediately followed by the setting's name again, then a newline. In words: "cmd DELc <name> $<name>". [read]
- The server relies on the client's console expanding the `$<name>` macro into the setting's current value when it runs the stuffed line. There is no protocol message for it. [read]
- The client's `cmd` command forwards the rest of the line to the server as an ordinary client string command (clc_stringcmd). So what comes back is the client command named DELc (0x7F, `c`) with argument 1 the setting name and the rest of the line (everything after the name, taken raw, spaces included) as the value. [read for the server side; the forwarding is the client's ordinary `cmd` behaviour, read in the Quake II source]
- The DEL byte keeps a player from typing the reply command by hand easily; nothing else authenticates the reply. A client may send a DELc line at any time with any value, and the server evaluates it against the bans exactly as if it were an answer. [derived]
- The reply command is one of the server's own built-in client commands: it is handled whatever the client's state (loading or in game) and never reaches the game mod. [read]
- The setting name in the reply is matched to bans ignoring case. [read]
- The same reply command carries other queries too: first argument `version` (the client's version string, kept the first time and, on a dedicated server, logged as `<name>[<addr>]: <version>`), `connect` (the forced-reconnect check), `actoken` (the anticheat token) and `console` (an admin's one-off query of a client's setting). After that special handling the reply is still checked against every setting ban whose name equals the first argument, so a ban on `version` works on the version answer. [read]
- Each ban checked on a reply that does not kick gives back one unit of the per-packet string command allowance (8 string commands per packet). The intent appears to be that answers do not use up the limit; with several bans on one name, more units are given back than the one command consumed. [read for the refund; intent derived]

## 2. When the check is made

- At the client's `new` request: that is, once at first connect and again on every map load (on a map change every connected client is put back to the connected state and sends `new` again). The queries are stuffed in the same burst as the rest of the `new` handling: on the first connect the version query (and anticheat token query) and the admin's connect stuffcmds first, then the forced-reconnect query if one applies, then one query per setting ban, then the user info bans are evaluated, then the client moves on to receiving the game state. [read]
- At `new`, a ban whose setting name is `version` (ignoring case) is not stuffed, because the version is already asked on the first connect. On a later map the version is therefore not asked again, so a `version` ban is not re-evaluated on map changes. [read; the consequence derived]
- The answers arrive whenever the client runs the stuffed lines, normally while it is loading the map; a kick can therefore land before or after the client enters the game. [derived]
- Not at `begin` (entering the game), and not on a timer. [read: no other caller]
- `checkcvarbans` (admin): answers "No server running." when no server runs and "No cvarbans registered." when the list is empty. Otherwise it stuffs one query per ban (including a ban on `version`, which is not skipped here) to every client that is fully in the game (spawned). Clients still connecting or loading are not reached. [read]

## 3. A client that never answers, or has no such setting

- The server keeps no record that a query is outstanding: no timeout, no default value, no action when no answer comes. A client that ignores the stuffed line is simply never checked. [read: nothing stores pending queries; derived: no consequence]
- A setting the client does not have: in the Quake II client, an unknown name expands to the empty string, so the reply carries an empty value. [read in the Quake II source] The server then compares the empty value: `*` does not match it, `!*` does, `=0` matches it (an empty value reads as the number 0), exact matches compare against "". [derived from the match rules]
- A client that does not expand macros would send the literal text `$<name>` as the value, which the server compares like any other value. [derived]
- In the Quake II client the expansion is skipped inside double quotes, and a value that itself contains a `$` is expanded again (up to 100 expansions per line before the line is discarded with "Macro expansion loop, discarded."). A line over 1024 characters after expansion is discarded with "Expanded line exceeded 1024 chars, discarded." [read in the Quake II source; the server's query line itself is never quoted]

## 4. The actions for a setting ban

- Accepted action words (case ignored): `log`, `print` (also spelled `message`), `stuff`, `kick`. `ignore` is not accepted for these bans (it prints the usage line). Any other word prints: `Usage: addcvarban <cvar> <match> [log|print|stuff|kick] [comment]` (the first word is the command as typed). [read]
- Default action when none is given: `log`. This differs from command filters, whose default is `ignore`. [read]
- The comment is everything from the fifth word on, joined; it is kept only when the action is `print`, `stuff` or `kick`. With `log` any extra words are dropped. The comment is optional for every action; the documentation calls it mandatory for print and stuff but the server does not enforce that. [read]
- On a match (in all cases the comparison is the match string against the reported value):
  - `log`: the server console gets `<name>[<addr>]: matched cvarban: "<var>" is "<value>"` (var as the ban spells it). Nothing goes to the client. Checking continues. [read]
  - `print`: no log line. If there is a comment, the client gets it as a high-priority print with a newline appended. With no comment nothing at all happens. Checking continues. [read]
  - `stuff`: no log line. If there is a comment, it is sent to the client as a stufftext with a newline appended, so the client runs it as a console line. With no comment nothing happens. Checking continues. [read]
  - `kick`: the same log line as `log`; then, if there is a comment, it is sent to the client as a high-priority print (not as the disconnect reason); then the client is dropped. The drop reason reads `<name> was kicked`: the client is printed that line; the other players are printed it too only if the client was in the game (spawned); a dedicated server's console prints `<name>[<addr>] was kicked`. Then the client gets the disconnect message. Checking stops. [read]
- Command filters behave the same way on a match except for their default (`ignore`, which does nothing) and their log line, which reads `<name>[<addr>]: issued banned command: <command>`. [read]

## 5. Deleting and listing

- `delcvarban` and `deluserinfoban` take one argument: a number, a name, or `all`. With no argument: `Usage: delcvarban <id|var|all>` (the command as typed). [read]
- An empty list answers `No cvarbans registered.` or `No userinfobans registered.` whatever the argument. [read]
- `all` (exact lower case; `ALL` is treated as a name) removes every entry and answers `Removed <n> cvarban.` when n is 1 and `Removed <n> cvarbans.` otherwise (`userinfoban` / `userinfobans` for the other list). [read]
- A number (all digits) is a 1-based position in the list order (the order added; numbering closes up after a removal). It removes that one entry and answers nothing on success. Out of range: `No such cvarban index: <n>` (`No such userinfoban index: <n>`). [read]
- Anything else is a name: every entry whose setting (or key) equals it, ignoring case, is removed, whatever its match or action, answering `Removed <n> cvarban(s)` as above. None: `No such cvarban string: <name>` (`No such userinfoban string: <name>`). [read]
- `listcvarbans` and `listuserinfobans` print `No cvarbans registered.` / `No userinfobans registered.` when empty; otherwise a two-line header, then one row per entry in list order. Columns: id (left-aligned in 2 characters, from 1), var (left-aligned, padded to 16), match (padded to 16), action (padded to 6: log, print, stuff or kick), comment (empty when none). Single spaces separate the columns; longer values are not cut, they push the row right. The header reads `id var              match            action comment` over a dashed rule of the same widths. Both lists use the heading `var`. [read]
- Adding a duplicate (same name and same match, both ignoring case, and same action) is refused with `Cvarban already exists: <words 1 to 3 as typed>` or `Userinfoban already exists: ...`. The same name and match with a different action is allowed. [read]

## 6. User info bans

- Three moments check them. [read]
  - At connect (the connect request, while the user info is parsed, after the name and password checks): only bans with action `kick` are considered, and only for matching; no log line is written. The first matching kick ban refuses the connect with an out-of-band print reading the ban's comment then a newline then `Connection refused.`; with no comment it reads `Userinfo banned.` then a newline then `Connection refused.` [read]
  - At every `new` (first connect and every map load), after the setting-ban queries: every action applies, as in question 4. A kick here drops the client before it is sent the game state. [read]
  - At every user info change the client sends after connecting (at most 8 user info updates per packet are taken), after the name has been validated: every action applies. A kick drops the client, and the game mod is not told of the change. Otherwise the change goes on to the game mod as usual. [read]
- Every check walks the whole user info, not only the changed key: a `print` or `stuff` ban on any key the client has is fired again at every change and every map. [read; consequence derived]
- A key the client's user info does not contain is never checked, so `!*` cannot catch a missing key. [read]
- At connect, when the game mod does not take extra user info, the server has already removed `mvdspec` and (if a password is set) `password`, and set an `ip` key to the client's address, before the check; so such a ban on `ip` applies at connect. [read]
- The log line for a user info ban is the same as a setting ban's: `<name>[<addr>]: matched cvarban: "<key>" is "<value>"`. [read]

## 7. Several bans, order, stopping

- Several bans may name the same setting (or key), with different matches or actions. [read]
- Setting bans: an answer is checked against every ban with that name, in the order added. [read] Each such ban was also asked separately, so n bans on one setting send n queries, and each of the n answers is checked against all n bans: the actions of a matching non-kick ban fire n times. [derived]
- User info bans: the keys are taken in the order they appear in the user info string; for each key, the bans in the order added. [read]
- Only a matching `kick` ban stops the check (the client is gone). `log`, `print` and `stuff` matches do not stop it; every later ban still runs. [read]

## 8. Macro expansion in stuffed lines in the stock clients

- Quake II: yes. Every console line the client executes, including stuffed text, is tokenized with `$<name>` expansion (a setting's value; empty for an unknown name; not inside double quotes). [read in id Software's published Quake II source; the 3.20 binary itself was not checked]
- QuakeWorld 2.40: no. Its console tokenizer has no macro expansion; a stuffed `$<name>` reaches the server literally. [read in id Software's published QuakeWorld source]

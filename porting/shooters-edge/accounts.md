# Accounts: player logins, accounts and match stats on the classic server

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of how the classic server program (MVDSV), the classic match mod (KTX) and the classic client (ezQuake) handle player logins, the accounts file, web logins, the end-of-match stats file, last scores, the frag logs and the status query, plus how Quake 2 clients and servers (q2pro, Yamagi Quake II, r1q2) treat console words, passwords, prompts and names. Written from their source by a reader session; the building session reads only this. No code, no internal identifiers. It extends `server-settings.md` §26, §27 and §29, which it does not repeat.

Read at these commits:
- MVDSV `4e63928`, KTX `b1b23a9`, ezQuake `c633fae` (all github.com/QW-Group).
- q2pro `601a8df` (github.com/q2pro/q2pro), Yamagi Quake II `03319cb` (github.com/yquake2/yquake2), r1q2 archive `dfe96b7` (github.com/tastyspleen/r1q2-archive).

Each fact is marked **[read]** (stated plainly by the source), **[derived]** (follows from what was read; re-ask before relying on it) or **[measured]** (none here: nothing was run).

Words: "the server" is MVDSV, "the mod" is KTX, "the client" is ezQuake unless named. "File mode" is `sv_login_web 0` (or a server built without the web library); "web mode" is `sv_login_web` 1 or 2. Message texts are exact; `\n` marks a line break and `<x>` a filled-in value.

**Corrections to `server-settings.md` §26, §27 and §29.**
- The web response command the server accepts is `login-response <r>`, not `challenge-response <r>`. The text `challenge-response` appears only in that command's usage line (`Usage: challenge-response <response>`). [read]
- `sv_login_web` defaults to 1 in a server built with the web library (it does not exist otherwise). [read]
- `*auth` and `*flag` are set only by a web login. A file login, password or address, changes no `*` key. [read]
- In-game `laststats` (the mod's command) and the connectionless `laststats` query (the server's) are different things; see 12 and 13. [read]

---

## A. The local login, as a stock client sees it

### 1. The state of a client that has not logged in

- The login is checked when the client asks for the level (its first `new` request), after the real-address check. Until it succeeds, the server stops there: it sends no level data, so the client has no map, cannot move and sees no world, only its console. [read]
- The client already holds a client slot (it counts against `maxclients`), and is in the connected state. [read] The server console has already printed `Client <name> connected` (or `Spectator <name> connected`). [read]
- Other players are not told about it: its userinfo goes out to other clients only once it has spawned, so it is not in their scoreboard. [read] The mod has not been told about it either (the mod's connect hook runs at spawn), so it is in no mod list and gets no mod messages addressed to players. [derived]
- It receives every broadcast print (chat, obituaries, server announcements), since broadcasts go to every client in the connected state or later. [read] It cannot chat: everything it says is taken as login input (see 2). [read]
- It does appear in the connectionless `status` reply, which lists every client from the connected state on, with frags 0 and its connect time and ping. [read]
- `sv_login 1`: a spectator skips the login entirely and is marked "not needed". `sv_login 2`: spectators must log in like players. [read]
- Timeout: a client still not logged in 60 s after its connect time is dropped with `Login timeout expired` (server console `Login time out for <name>`). The connect time is reset whenever a `new` request arrives in the connected state. [read] The 60 s is fixed, not a setting. [read]
- With `sv_registrationinfo` set, its text is printed to the client just before the prompt. [read]

### 2. How a stock client answers the prompt

**What the server takes as login input.** Only `say` lines (not `say_team`) from a client that is not logged in. They never reach the mod or other players. [read] Any other command is handled as usual: an unknown word gets `Bad user command: <word>`. [read]

**How a stock ezQuake player gets a line there.** [read]
- A console line with no leading slash whose first word is not a known command, setting or alias is sent as `say "<line>"`. This is ezQuake's default chat mode, `cl_chatmode 2`. Mode 1 sends every line as chat. Mode 0 runs every line as a command.
- The chat key (`messagemode`) also sends `say`.
- A line with a leading slash, or whose first word is a known command, runs as a client command. An unknown command typed that way is not sent to the server. ezQuake prints `Unknown command "<word>"` (when `cl_warncmd` is 1, the default) and drops it.
- So a player types `niclas secret` (or `say niclas secret`) in the console. `/niclas secret` fails locally. [derived]

**How the line is split.** [read]
- With two or more words as separate arguments (unquoted `say a b`), the first is the login and the second the password. Further words are ignored.
- With one argument, which is what ezQuake sends because it wraps the line in quotes, the text is cut at the first space. The part before it is the login, and the part after it, with leading spaces dropped, is the password.
- When a login has already been taken from an earlier line, the first word of the new line is the password and the rest is ignored.
- A password containing a space therefore cannot be entered, and account passwords cannot contain one anyway (see 7). [derived]

**Each step and what the player sees** (all private prints at the high print level). [read]
- Prompt on connect: `Enter login & password:` (file mode) or `Enter username:` (web mode).
- A line with a login and no password: the login is remembered and the player sees `Enter password for <login>:`. The next line's first word is the password.
- A wrong password, or a login that has no account: `Access denied\nPassword for <login>:`. The two cases look identical. [read]
  - The remembered login is kept, so every later line is tried as a password for the same login. A player who mistyped the login cannot correct it without reconnecting or waiting for the 60 s timeout. [derived]
  - A failure is counted only against an existing account. [read]
- A blocked account: `Login blocked`, then the client is dropped. [read]
- The account is already logged in (file mode): `Login in use!\ntry again:`. The remembered login is cleared, so the next line starts over with login and password. This check comes before the password check, so it shows that the account is in use without proving the password. [read]
- The 10th failure in a row blocks the account. The server console prints `account <login> blocked after 10 failed login attempts`. The player sees `Access denied...` on that attempt and `Login blocked` plus a drop on the next. [read] Failures reset to 0 on a success. [read]
- Success:
  - Everyone sees the broadcast `<name> logged in as <login>`. It is not sent when the client is a spectator and a game is in progress. [read]
  - The client, if not yet spawned, then sees `Welcome <login>`. [read]
  - With `sv_forcenick`, the name is then forced (see 4). [read]
  - The server sends the client `cmd new`, which restarts the level request, and the connection carries on normally. [read]
- The client's stored login is at most 15 characters. When the login and password come in one line, the full typed login is checked. When they come in two lines, the stored (cut) login is checked. So an account name longer than 15 characters can only log in with login and password on one line. [derived]
- The server's own chat flood protection (`floodprot`) runs before the login check, so login lines count toward it. A flooding client gets the usual flood messages and its line is not tried. [derived] The mod's flood protection does not apply, because the mod never sees these lines. [derived]
- A login line that starts with `.qw ` is treated as a cross-server broadcast before the login check. [derived]

### 3. Is the password hidden?

- **Server side.** [read]
  - A login line is handled before the server prints or logs chat, so it is not echoed to other players, not written to the console log, not put in the server demo (MVD) and not sent to QTV or broadcast streams.
  - Only the `logged in as` broadcast reaches demos and streams, and it names the login, not the password.
  - The prompts and replies are private prints, which do not go into the MVD.
- **On the wire.** The line travels in clear text inside the client's normal command stream. Nothing is encrypted or hashed by the client. [read]
- **Client side.** [read]
  - ezQuake does not print a console line that it turned into chat. This is `con_hide_chat_input 1`, the default.
  - The line stays in the console's input history. Whether that history is saved to disk was not checked.
  - A client demo records what the server sends, not what the client types. [derived]
- **Stored form.** With `sv_hashpasswords 1`, the accounts file holds an unsalted SHA-1 of the password, as 40 upper-case hex digits. [read]
- **Admin commands.** An admin who creates an account over rcon has the whole rcon line, password included, written to the rcon log when rcon logging is on. [read]

### 4. What changes after success

- **File login** (password or address): no userinfo or serverinfo key changes. [read]
  - The login is visible only to the mod (it can read a player's login by name) and in the stats file. [read]
  - The `status` query marks the player as logged in (see 16). [read]
- **Web login**: `*auth` is set to the login and `*flag` to the flag (see 10). [read]
- **`sv_forcenick 1`.** The forced name is the web alias if there is one, otherwise the login. [read]
  - Every other client whose name matches it (ignoring case and red text) is kicked with the reason ` (using authenticated user's name)`.
  - If the player's own name already matches, nothing more happens. Otherwise the server sets its `name` and sends the client the command `name <forced name>`.
- **Broadcast.** Everyone sees `<name> logged in as <login>` (see 2 for the spectator exception). With a web login, a later logout broadcasts `<name> logged out`. [read]

### 5. Address accounts

- **In the file**: a line `<address> <username> <state>` with no failure count. The address is the account's key. [read]
- **Creating one**: `acc_create <address> <username>`. The address is anything the server's address-filter parser accepts. [read]
- **Matching**: the client's real address, written as four dotted decimal numbers, is compared with the stored key as text. [read] So only a plain `a.b.c.d` key ever matches, and a key written with a mask never logs anyone in. [derived]
- **What happens**: the check runs when the login is checked, before any prompt. On a match the client is logged in at once, with the username as its login. [read]
  - The player sees nothing: no `Welcome`, no broadcast, and no `sv_forcenick` rename. [read]
  - A blocked address account does not drop the client. It falls through to the normal prompt. [read]
- **Hashing**: the username is never hashed. [read]
- **Sharing**: any number of clients from one address can be logged in to it at once. [read]
- **Web mode**: an address match logs the client in locally, but a web-mode server still requires a web login before letting it in. No prompt is shown in that case, so the client waits in the connected state. [derived]

### 6. The same account twice

- **File mode, login accounts**: a second login while the first is connected gets `Login in use!\ntry again:`. [read] Each account counts how many clients use it, and that count falls when a client drops. [read]
- **Address accounts**: allowed any number of times. [read]
- **Web mode 1**: there is no in-use check, so the same web account can be in twice. [read]
- **Web mode 2**: the local account is checked with the in-use rule, so a second web login of the same account is refused with `Login blocked` and dropped. [derived]

### 7. The `acc_*` commands and the file

**The file** is `accounts` in the game directory. [read]
- Login accounts are written `<login> <password> <state> <failures>`, and address accounts `<address> <username> <state>`.
- Each field is read as at most 40 characters.
- States: 0 is a free slot (skipped on load and never written), 1 is in good standing, 2 is blocked. [read]
- The file is read at every map load. [read] On reload, every logged-in file-mode client is checked against it again. [read]
  - A client whose account is gone or blocked is logged out silently and is not dropped.
  - A client whose login account is now in use by another is also logged out.
- The file is rewritten after every create, remove, block or unblock, and after every failed password. [read]
- Reading stops at the first malformed line, with `Error reading account data`. [read]
- At most 1000 accounts. [read]

**Valid login names**: 3 to 41 characters of letters, digits, `.` and `_`. [read] A 41-character name is stored cut to 40. [derived] The usage text says 40.

`acc_create <login> [<password>]` or `acc_create <address> <username>`
- With no argument: the usage `usage: acc_create <login> [<password>]\n       acc_create <address> <username>\nmaximum 40 characters for login/pass`. [read]
- An address with no username: `usage: acc_create <address> <username>\nmaximum 40 characters for username`. [read]
- Errors: `MAX_ACCOUNTS reached`, `Invalid login!`, `Login already in use`. [read]
  - `Login already in use` compares names ignoring case. For an address account, the username is also checked against existing keys. [read]
  - `Invalid pass!` is checked only when a fourth argument follows the password. A normal two-argument create does not check the password's characters at all. [read]
- With no password, the password is the login itself. [read]
- With `sv_hashpasswords 1`, a login account's password is stored as its SHA-1. [read]
- Success: `login <login> created`. [read]

`acc_remove <login>`
- No argument: `usage: acc_remove <login>`. Not found: `account for <login> not found`. Found: `login <login> removed`. [read]
- With `sv_login 1` and the account required locally (file mode or web mode 2), every client whose login matches is logged out and dropped. With `sv_login 2` nobody is dropped. [read]
- An address account is removed by its address. Its users' logins are the username, so they are not dropped. [derived]

`acc_list`
- `account list is empty`. Otherwise `account list:`, then one line per account: the first 16 characters of the login, a space, and `blocked` or nothing. Then `<n> login(s) found`. [read]

`acc_block <login>`
- No argument: `usage: acc_block <login>`.
- Found: the state becomes 2, the server prints `account <login> blocked`, and the first connected client using that login is dropped (only one). [read] Blocking an already blocked account says the same.
- Not found: `account <login> not found`. [read]

`acc_unblock <login>`
- No argument: `usage: acc_unblock <login>`.
- Blocked: the state becomes 1, the failure count is cleared, and the server prints `account <login> unblocked`.
- Not blocked: `account <login> not blocked`. Not found: `account <login> not found`. [read]

All names are matched ignoring case. [read]

### 8. Changing side, map changes and reconnects

- **Spectator to player** (`join`): with `sv_login` non-zero and the client not logged in, it is refused. [read]
  - File mode: `This server requires users to login.  Please disconnect and reconnect as a player.`
  - Web mode: `This server requires users to login.  Please authenticate first (/cmd login <username>).`
  - So under `sv_login 1` a spectator who skipped the login can never join without reconnecting as a player. [derived]
- **Player to spectator** (`observe`): no login check. [read]
- **Map change**: the login is kept, because the client stays connected. When the server sees an existing login at the level request, it skips the prompt. [read] The account file is re-read and re-checked (see 7). [read]
  - If `sv_login` is 0 at the level request, a file login is logged out and a web login is kept. [read]
- **Reconnect**: dropping a client always logs it out. `*auth` and `*flag` are cleared and the account's in-use count falls. A reconnecting client logs in again (an address account does so silently). [read]

---

## B. The web login and the client's side

### 9. How a client produces the response

**The server never sees a password.** [read]
1. The player's username goes to the web service, which returns a challenge.
2. The server sends the client the hidden line `//challenge <c>`. A line starting with `//` is a comment to a stock command parser, so nothing shows. [derived]
3. The client must answer with `cmd login-response <r>`.

**ezQuake does it by itself, through the web service, from a token file.** [read]
- `cl_username <name>`: the player sets this. The name may hold letters, digits and `_ [ ] ( ) . -`.
  - The client loads the file `<name>.apikey` from its configuration directory; its whole content is the login token.
  - Errors: `Illegal character <c> in username`, `Unable to load authentication token for '<name>'`.
  - Setting the name while connected sends `cmd login <name>`. Setting it empty logs out (`cmd logout` when logged in) and forgets the token.
  - Changing it while logged in is refused with `You are logged in to the server & must logout first`.
- `authenticate`: the command a player types.
  - Without a token: `You must load a login token by setting cl_username first.` and `Login tokens must be located in your configuration directory`.
  - Not connected: `Cannot authenticate, not connected`.
  - Already logged in: `Logging out...` and `cmd logout`.
  - Otherwise `Starting authentication process...` and `cmd login "<name>"`.
- When the server sends the hidden prompt `//authprompt` and the client has a token and a username, it sends `cmd login <name>` without being asked.
- **The response.** When `//challenge <c>` arrives and a token is loaded, the client posts a form to `<cl_www_address>Authentication/GenerateResponse`. `cl_www_address` is a read-only setting whose default is `https://badplace.eu/`.
  - The form has three fields: `hashalg` = `sha1`; `authKey` = the SHA-1 of the challenge text, a `^`, then the token, written as 40 upper-case hex digits; `challenge` = the challenge text.
  - The service answers `Result`, `Response` and `Message`. On `Result:Success` the client prints `Responding to challenge...` and sends `cmd login-response <Response>`.
  - Failures print `Error receiving challenge response: <Message>` or `...: unknown error`.
  - With an empty address: `cl_www_address not configured: please authenticate manually`.
  - So the token is the secret, and the challenge response itself comes from the service, not from a hash the client computes alone.
- If the token file ends with a newline, that newline is part of the hashed token. [derived]

**A client without that support.** [derived]
- It never sees the challenge, so it cannot compute or fetch a response in the ordinary way.
- What the server accepts:
  - Before spawning: the first `say` line is taken as the username (its first word). The next `say` line is taken as the response.
  - In game: `cmd login <user>`, then `cmd login-response <r>`.
- The player would need to get the challenge and the response by other means. The sources give no manual path.

**The server's side, step by step.** [read]
- Before spawning, the username comes as a `say` line (its first word). Or, from a client that is spawned, as `cmd login <user>`, which needs exactly one argument (`Usage: login <username>`).
- One request per 5 s per client. Earlier ones get `Please wait and try again`.
- If the client is already logged in by web: `You are already logged in as '<login>'`.
- If the client is not spawned and `sv_login` is 0: `Cannot login during connection`.
- The server prints `Generating challenge, please wait...` (only on the say path), posts `userName` and `status` (`0` during the connect login, `1` in game) to `Authentication/GenerateChallenge`, and on `Result:Success` stores the challenge, prints `Challenge stored...` and sends `//challenge <c>`.
- The response line (`say` before spawning, or `login-response <r>`) is posted as `challenge` and `response` to `Authentication/VerifyResponse`. The say path also prints `Challenge received, please wait...`.
- `login-response` with no stored challenge: `Please wait and try again`. With a wrong argument count: `Usage: challenge-response <response>`.
- With `sv_www_address` empty: `Remote logins not supported on this server`.
- `logout` (a command only in web-capable servers): for a web login it broadcasts `<name> logged out` and clears the login. Then, for any client that must be logged in (`sv_login 2`, or `sv_login 1` as a player), it drops the client, whatever kind of login it had. [read]

### 10. What `*auth` and `*flag` hold and where they show

- `*auth` is the `Login` value the service returned. `*flag` is its `Flag` value, or empty. Both are set only by a web login and are cleared on logout or drop. [read]
- Both are among the userinfo keys sent to every client and recorded in demos, like the name. [read]
- `*flag` is a short code that the client looks up, ignoring case, in a flag-image list. Country codes are the evident intent, but the sources do not define the codes. [read]
- **ezQuake's scoreboard** (for any player with `*auth` set): [read]
  - The login name replaces the player's name, drawn in `scr_scoreboard_login_color` (default `255 255 192`), when `scr_scoreboard_login_names` is 1 (the default).
  - Before the name it draws the flag image for `*flag`, or, with no flag or no image, the text `scr_scoreboard_login_indicator` (default a yellow `*`).
  - The flag list is `textures/scoreboard/<scr_scoreboard_login_flagfile>.json` (default `flags`). It is a JSON array of objects with `code`, `file`, `x`, `y`, `width` and `height`: one image or a cell of an atlas per code.
- **Status query**: the login never appears in it. Only the flag does, with option bit 64 (see 16). [read]
- **Hubs**: ezQuake's server browser asks `status 23`, which has no flags, so it shows no logins or flags. [read] What other hubs show is not in these sources.

### 11. The web service's answers

- **Shape.** A response is lines of `Key:Value`. [read]
  - A value continues onto the next line when its line ends in `$`. The `$` becomes a space. [read]
  - Unknown keys are ignored. At most 8192 bytes are accepted. [read]
- **GenerateChallenge**: `Result`, `Challenge`, `Message`. [read]
  - `Result:Success` with a `Challenge` stores it.
  - Otherwise, with a `Message`, the player sees `Error: <Message>`, and with neither, `Error: unknown error`.
  - The client is not dropped at this step. [read]
- **VerifyResponse**: `Result`, `Login`, `Alias`, `Flag`, `Confirmation`, `Message`. [read]
  - On `Result:Success`:
  - `Login` becomes the client's login and `*auth`. `Flag` becomes `*flag`. `Alias` (or `Login` when absent) is the name `sv_forcenick` uses. `Confirmation` is kept as the token the stats upload sends.
  - The local account file is then consulted. A blocked local account gives `Login blocked` and a drop. In web mode 2, a missing local account does the same. Otherwise the login completes as in 2.
  - On failure: `Error: <Message>` (or `Error: unknown error (invalid response from server)`), then `Challenge response failed.`. A client that has not spawned is then sent `Login blocked` and dropped. A spawned client keeps playing, not logged in. [read]
- **What the service says** for a bad response or an unknown user (the `Message` texts, any HTTP status) is not in these sources. They hold only the reading side.
- **Web mode 2 and the local account.** The local account is checked with the web login standing in as the password. [derived] So that check passes only for an account created with no password (password equals login). Any other account fails it, which gives `Login blocked` and also counts a failure toward the 10-failure block. [derived]
  - In web mode 1, an existing local account is not password-checked. Only its blocked state matters. [read]

---

## C. Match stats the mod writes

### 12. The stats file

**When and where.** [read]
- It is written at a match's real end (see 14). It is written only when the server's demo is recording and `sv_demotxt` is 2. It also needs the server's own address and port to be known.
- The file is named `demoinfo_<ip>_<port>.json` in the mod's directory, and is overwritten each time.
- After writing it, the mod uploads it to the web service when `sv_www_address` is set. With a server that can embed info in its demo stream, it also embeds the JSON in the demo.
- It then appends the file in the format `k_demotxt_format` names (`json` or `xml`; default `xml`, which is written as a second file beside the JSON) to the current demo's `.txt`.
- So the demo's `.txt`, which the `laststats` query returns, is the XML version unless `k_demotxt_format` is `json`. [derived]

**Text.** The JSON is written on one line with no line breaks. [read] String values are escaped: backslash, quote, and the control characters `\n \r \b \t \f` get their two-character escapes. Any other byte below 32 or from 128 up becomes `\u00XX`. A string is cut after about 1000 output characters. [read] The `login` value is written without escaping. [read]

**Top level** (in this order; `[opt]` means present only when set):
- `version`: 3. [read]
- `date`: local time as `YYYY-MM-DD HH:MM:SS +ZZZZ`. Empty when the date fails. [read]
- `map`: the map's name. `hostname`: the server name with the high bit stripped from each byte. `ip`, `port`: the server's own address and port. [read]
- `matchtag` [opt]: the serverinfo match tag. `mode` [opt]: one of `instagib`, `midair`, `hoonymode`, `race`, `clan-arena`, `rocket-arena`, `duel`, `team`, `ctf`, `ffa`, `unknown`, picked in that order of precedence. [read]
- `tl` [opt]: the time limit in minutes. `fl` [opt]: written when a frag limit is set, but its value is the time limit (a defect). `dm` [opt]: the deathmatch mode. `tp` [opt]: the teamplay mode. [read]
- `duration`: whole seconds from match start to the moment of writing. [read]
- `demo` [opt]: the demo file's name. [read]
- **Race** maps write `race` instead of the team and player lists (see below). [read]
- `teams` [opt]: an array of team names (strings), present when any team was counted. [read]
- `players`: an array of player records. [read]
  - Players are grouped by team, each team's players together, teams in the order first met.
  - Disconnected players (kept as "ghosts" during the match) are included. [read]

**Each player record:**
- `top-color`, `bottom-color`: numbers. `ping`: ms. [read]
- `login`: the player's login (any kind, see 4). Empty when not logged in. [read] A ghost's login is not settled by the source.
- `name`, `team`. [read]
- `stats`: [read]
  - `frags` (the score), `deaths`, `tk` (team kills).
  - `spawn-frags`: kills of a victim within 2 s of its spawn.
  - `kills`, `suicides`.
- `dmg`, in health points: [read]
  - `taken`: from enemies.
  - `given`: to enemies.
  - `team`: to teammates.
  - `self`.
  - `team-weapons`: to teammates holding a rocket launcher or lightning gun.
  - `enemy-weapons`: the same to enemies.
  - `taken-to-die`: damage taken divided by deaths, whole number, 99999 with no deaths.
- `xferRL`, `xferLG`: team modes only. The number of this player's dropped rocket-launcher or lightning-gun packs that a teammate picked up. [read]
- `spree`: [read]
  - `max`: the longest run of kills of non-teammates without dying.
  - `quad`: the same, counting only kills made while holding quad damage.
- `control`: seconds. In duel, a player gains "control" on the second kill of a spree and loses it on death or disconnect. The value is the total time held. [read]
- `speed`: `max` and `avg` of horizontal speed in units per second, sampled every server frame while the match is running. [read]
- `handicap` [opt]: present when it is not 100. [read]
- `weapons`: an object keyed by `axe`, `sg`, `ssg`, `ng`, `sng`, `gl`, `rl`, `lg`. A weapon appears when it has any attacks, deaths, drops, spawn pickups or pickups. Each holds: [read]
  - `acc` [opt]: `attacks` (shots, counted per pellet for the shotguns) and `hits`. Plus `real` and `virtual` hits for the rocket and grenade launcher. A real hit did damage, while a virtual hit would have hit with full health.
  - `kills` [opt]: `total`, `team`, `enemy` and `self`. `enemy` reads 0 for axe and shotgun, in instagib, and in deathmatch mode 4 and up.
  - `deaths`: deaths to this weapon, written only below deathmatch mode 4.
  - `pickups` [opt], below deathmatch mode 4: `dropped`, `taken` (picked up when not already owned), `total-taken`, `spawn-taken` and `spawn-total-taken` (the spawn variants exclude backpacks).
  - `damage` [opt]: `enemy` and `team`.
- `items`: an object keyed by `health_15`, `health_25`, `health_100`, `ga`, `ya`, `ra`, `q`, `p`, `r` (mega health, the three armours, quad, pentagram, ring). [read]
  - Each holds `took` (count) [read] and `time` (whole seconds held) [read]. For a powerup the time runs until it ran out or the holder died, and for armour until it was replaced or the holder died [derived].
- `ctf` [opt, CTF only]: `points`, `caps`, `defends`, `carrier-defends`, `carrier-frags`, `pickups`, `returns`. Then `runes`, an array of four whole seconds held: resistance, strength, haste, regeneration. [read]
- `instagib` [opt]: `height` (a pair: total and maximum), `rings`, and `gibs`. `gibs` holds `coil`, `axe`, `stomp`, `multi`, `air` and `best-multi`. [read]
- `midair` [opt]: `stomps`, `midairs` (`bronze`, `silver`, `gold`, `platinum`), `total`, `bonus`, and `heights` (`total`, `max`, `avg`). [read]
- `ra` [opt, rocket arena]: `wins`, `losses`. [read]
- `hm-rounds` (a text of round results, duel hoonymode) or `hm-frags` (an array of frags per point, other hoonymode). [read]
- `lgc` [opt, lightning-gun challenge]: `under`, `over`, and `hits` and `misses` as arrays of 20 distance buckets. [read]
- `bot` [opt, bots]: `skill` (level) and `customised` (true or false). [read]

**Race** (instead of teams and players): [read]
- `race` holds:
  - `route`: from 0.
  - `weapon-mode`: `allowed`, `delayed` or `none`.
  - `can-false-start`: true or false.
  - `match`: true or false.
  - `pacemaker` [opt]: `time` in seconds and `name`.
- In race match mode it adds `scoring` and a `racers` array of records with `bestTime` (s), `completions`, `score`, `racer`, `distance`, `time` (s) and `wins`.
- Otherwise `racers` holds `avgspeed`, `distance`, `time` (s), `racer` and `maxspeed`.

### 13. `lastscores`

There are two, with the same name.

**The server's `lastscores`.** It is a connectionless query, a player command and a console command (the console one is `sv_lastscores`). [read]
- **Request**: the connectionless text `lastscores [<n>]` after the usual four `0xFF` bytes. It is answered only when `sv_allowlastscores` is 1 (the default). [read]
- **Reply**: an out-of-band print packet. [read]
  - It starts with `List of <n> last demos:`, then one line per demo, oldest of the window first: `<k>. ` and the first line of that demo's `.txt` (up to 511 bytes), or `(empty)`.
  - The text is drawn gold: digits become the gold digit glyphs and other printable characters get the high bit.
  - With no demos: `No demos.`.
- **Count**: [read]
  - The default is 10. `<n>` of 0 or less also means 10, although the usage text says 0 means all.
  - The count is capped at the number of demos. It is capped at 10 while a game is in progress, with the note `<numlastdemos> was decreased to 10: match is in progress.`, and at 100 for a connectionless query, with `...decreased to 100: command from connectionless packet.`.
  - More than one argument gives the usage text.
- **What it lists**: it keeps nothing of its own. It lists the demo directory by the demo name pattern, sorted by date. [read]
- **The first line of a `.txt`**: [read]
  - With `sv_demotxt 1`, the server writes one score line:
  - Duel: `duel: <a> vs <b> @ <map> - <fa>:<fb>`.
  - Free for all: `ffa:` then `  <name>(<frags>)` per player, then ` @ <map>`.
  - Team play: `tp:` then `<team>[ <name>(<frags>) ...](<team total>)  ` per team, then `@ <map>`.
  - Then a block of `date ...`, `map ...`, `teamplay ...`, `deathmatch ...`, `timelimit ...` and the players.
  - With `sv_demotxt 2`, the first line is the start of the mod's stats file (one long line of XML or JSON). [derived]
- **The server's `laststats` query** (connectionless only, also gated by `sv_allowlastscores`). [read]
  - It replies `laststats <n>`, `[`, the full `.txt` of each of the last `<n>` demos (non-empty ones only) separated by `,` and a line break, then `]`.
  - `<n>` defaults to 10, and 0 or anything above 50 means 50. With no demos it replies `laststats 0:` and `[]`.
  - The result is a JSON array only when the `.txt` files hold JSON. [derived]

**The mod's `lastscores`.** [read]
- It keeps the last 30 results in a ring, set at each real match end. Ended or broken counts as real when the mode below applies.
- Mode tags are `duel`, `team`, `FFA`, `CTF`, `RA`, `Clan Arena`, `Wipeout`, `HoonyMode` and `race`.
- What is kept:
  - Free for all and rocket arena: the top two names and frags.
  - Duel and hoonymode: the two duellers.
  - Team games, CTF and clan arena: the two team names, team scores and member lists, and a third team for three-team modes.
  - Nothing is kept when either side is empty.
- Each entry also keeps a score line: scores as `<s1>:<s2>`, `OT` when there was overtime, the map name (8 characters), and the date (`Mon DD, HH:MM:SS YYYY`, cut to 13 characters).
- The player command is `lastscores` (or `lastscoresktx`), with an optional argument for the extended view, which adds team members.
  - A heading `<e1> vs <e2> <mode>` is printed whenever the mode or the sides change, then each score line.
  - It ends with `Lastscores: <n> entry found`. The plural is broken: it always says `entry`. With no entries: `Lastscores data empty`.
- Because the server's player command of the same name runs first, a player who types `lastscores` gets the server's demo list. The mod's list is reached as `lastscoresktx`. [derived]
- At each such match end the mod also sends one client the hidden line `//finalscores "<date>" "<mode>" "<map>" "<e1>" <s1> "<e2>" <s2>`. [read] The intent is to put the result in demos and QTV streams, where a client can show it. [derived]

### 14. When is a match counted?

- **The end.** [read]
  - Stats and the stats file come from the mod's match end. It runs when a match in countdown or in progress ends for any reason: the time or frag limit, a `break` vote, an admin stop, the server emptying, or a mode change.
  - The end is skipped when `deathmatch` is 0.
  - In hoonymode, only the final point counts.
- **Modes.**
  - Duel, team, CTF, free for all, rocket arena, midair, instagib and race all write the file. [read]
  - Clan arena (and wipeout) does not: it skips the stats step, so no stats file, though it does record a `lastscores` entry. [read]
  - Race also writes one per recorded race when a race recording stops. [read]
- **Breaks.** A match that is broken off goes through the same end. [read]
  - The file is written, uploaded and appended to the demo's `.txt` before the demo is stopped.
  - The demo, with its `.txt`, is then deleted when the match ran shorter than `k_demo_mintime` (seconds, clamped to 0..3600, where 0 means 120) and has no match tag.
  - With bots playing, the match tag does not protect it. A match that never started has nothing to keep.
  - So a short broken match still reaches the web service but leaves no demo or `.txt`. [derived]
- **No demo, no file.** A match with no demo recording (for example matchless, or a pickup when the server does not record) writes no file. [derived]
- **Bots.** They are counted like players, with a `bot` object. [read]
- **Pickups.** Pickup games and countdown-started matches are treated alike. There is no separate rule for pickups. [derived]

### 15. The frag logs

**The frag log** (`fraglogfile` turns it on; it writes files `frag_<port>_<NNNN>.log` in `sv_logdir`, a new file each time). [read]
- It takes a line each time the mod reports a frag. The mod does so for kills, suicides and some world deaths, with the victim given as both sides for a suicide.
- `frag_log_type 0`: `\<killer>\<victim>\`.
- `frag_log_type 1`: `\frag\<killer>\<victim>\<killer team>\<victim team>\<Y-M-D h:m:s>\`. The numbers are not zero-padded and the time is local.
  - Each map change also writes `\newmap\<map>\\\\<date>\`.
- The same lines feed the connectionless `log` query. Its reply is `stdlog <seq>`, then the last buffered block. A request whose number matches the last block, or a server not logging frags, gets a one-byte "no" reply.

**The mod frag log** (`modfraglogfile`, files `modfrag_<port>_<NNNN>.log`). [read]
- It does not use the mod's frag reports. It matches every broadcast print line against a list of obituary patterns (regular expressions).
- **The line**: `<first>\<second>\<weapon>\<unix time>`. The two-player patterns list the killer first and the victim second. The one-player patterns (suicides and world deaths) put the victim's name in both. [derived from how the patterns map captures]
- **Not recorded**: team, map and the victim's location.
- **Weapon tokens**: `die`, `axe`, `sg`, `ssg`, `ng`, `sng`, `gl`, `rl`, `lg`, `rail`, `drown`, `trap`, `tele`, `dschrg`, `squish`, `fall`, `team`, `stomps`.
- **System lines**: a second category, `<name>\<event>\<unix time>`, has the events `start`, `end`, `connect`, `disconnect` and `timeout`. No built-in pattern produces them.
- **The pattern list**: 60 built-in patterns. `sv_mod_msg_file <path>` replaces them with up to 512 lines of the form `<type>#<weapon or event number>#<1 or 2 players>#<reverse 0/1>#<regular expression>`.
  - `type` is 0 for a weapon and 1 for a system line. The numbers index the token lists above, in that order.
  - `reverse` swaps which capture is the killer.
  - The first matching pattern wins. A bad path prints `WARNING: sv_mod_msg_file_OnChange: can't open file <path>.` and keeps the built-in list.

---

## D. The status query

### 16. Logins in `status`

- **The request** is `status <bits>`. Each player line is: user id, frags, minutes connected, ping, `"name"`, `"skin"`, top colour, bottom colour. Option bits add fields in this order. [read]
  - 16 adds `"team"`.
  - 64 adds the flag field.
  - 128 adds `"b"` or `"h"`, for bot or human.
- **The other bits**: 1 is serverinfo, 2 players, 4 spectators, 8 spectators shown as players with frags `S`, 32 QTV streams. 0 is the old style, serverinfo plus players. [read]
  - Without bit 8, spectators show frags `-9999`, a negative ping and a `\s\` before the name. [read]
- **The flag field** (bit 64): [read]
  - The web `*flag` code in quotes, when there is one.
  - Otherwise `"none"` when the player is logged in by any means: file, address or web.
  - Otherwise `""`.
  - This is the only place a file login is visible from outside. [derived]
- **The login name** is never in the status reply. [read]
- `status 23` (1 + 2 + 4 + 16) carries no login information. [read]
- **ezQuake's browser** sends exactly `status 23`, so it shows none. [read]
- The QTV user list and other queries carry no login. [read] A hub that wants logins must ask with bit 64 and can learn only the flag, or whether a player is logged in. [derived]

---

## E. Quake 2 clients and servers

### 17. An unknown console word

**Quake 2, stock behaviour** (Yamagi Quake II, which keeps the id release's rules). [read]
- A console line is run as a command line, with or without a leading slash or backslash. There is no chat mode, and the line is echoed in the player's own console.
- A word that is not a command, alias or setting is sent to the server as a client command only when the client is fully in game. The text sent is the word, a space and the rest of the line, unchanged.
- In any earlier state, including while connected and loading, the client prints `Unknown command "<word>"` and sends nothing. A word starting with `+` or `-` is never sent.
- `cmd <text>` sends `<text>` as typed whenever the client is connected.
- So `login niclas` reaches the server only from a player already in the game. During connection the player must type `cmd login niclas`. [derived]

**On the stock server.** [read]
- A client command that is not one of the server's own goes to the game module, and only while a level is running.
- The stock game ignores it until the player has entered the game.
- Once in, the stock game turns any word it does not know into public chat. So `login niclas secret` sent to a stock game would be broadcast as `<name>: login niclas secret`.
- A server that takes logins this way must catch the word before the game does. [derived]

**q2pro.** [read]
- The same forwarding rule: only when fully in game, otherwise `Unknown command "<word>"`. A known server-only command typed before that gives `Can't "<word>", not connected`.
- Its console adds `con_auto_chat` (default 0). With 1 or 2, a line with no slash typed while in game is sent as `cmd say <line>` or `cmd say_team <line>`.

**ezQuake (QuakeWorld)**, for comparison (see 2). [read]
- A console line whose first word is unknown is sent as `say "<line>"` in the default chat mode 2, from the connected state on.
- An unknown word typed as a command (slash, or chat mode 0) is not sent at all: `Unknown command "<word>"`.
- `cmd <text>` forwards `<text>` from the connected state on, with the first word lower-cased.

### 18. Accounts on Quake 2 servers

No account or login system was found. [read]
- **Where it was looked for**: q2pro's server, r1q2's last public release, and the stock server and game. None has an accounts file, login commands, a login prompt or per-player authentication.
- **What they have instead** is shared passwords carried in the client's userinfo. [read]
  - The `password` key against the server password (`sv_password` in q2pro and r1q2, `password` in the stock game). q2pro refuses with `Please set your password before connecting.` or `Invalid password.`. The stock game refuses with `Password required or incorrect.`.
  - q2pro's reserved slots (`sv_reserved_password`, the same `password` key).
  - The stock game's `spectator_password` against the `spectator` key: `Spectator password required or incorrect.` at connect, or `Spectator password incorrect.` when switching to spectator in game.
  - q2pro removes the `password` key from the userinfo before the game sees it, when a server password is set.
- Other Quake 2 servers and mods were not read.

### 19. Showing a Quake 2 client a prompt

**The ways.** A server can print to the client's console (with a print level, where the chat level plays the talk sound) or send a centre print. [read]
- **Console prints**: shown while connecting and loading (the console is up), and in game on the top-of-screen notify lines. [read]
  - Console lines wrap at the console width: the screen width over the console scale, divided by 8, minus 2 characters. That is 38 at 320 pixels wide and 78 at 640 with scale 1. [read]
  - One message string is read up to 2048 bytes. [read]
- **Centre prints**: drawn only in game, not while connecting. [read]
  - Stock behaviour: each line of a centre print is drawn up to its first 40 characters, centred, and the rest of that line is not drawn. The text is held up to 1024 bytes and shown for `scr_centertime` seconds (default 2.5). It is also echoed to the console between rule lines. [read]
  - q2pro holds up to 1016 characters per centre print, draws its lines centred without the 40-character cut, and echoes it to the console. [read]

**So a login prompt for a connecting Quake 2 client must be a console print.** The safe line length is 38 characters, the width at 320 pixels. A centre print in game can be used too, with lines of at most 40 characters. [derived]

### 20. Player names in Quake 2

- The name is the userinfo `name` value. Userinfo values are under 64 characters, and the whole userinfo under 512. q2pro refuses `\`, `"` and `;` in any key or value. [read]
- **The server's own copy**: [read]
  - q2pro keeps 15 characters with the high bit masked off. The stock server keeps 31, masked the same way.
  - q2pro refuses a connect whose name is empty or all white space: `Please set your name before connecting.`.
- **The stock game** keeps 15 characters of the name, high bits included, for the scoreboard and obituaries. [read]
- **Colour**: [read]
  - There are no colour codes in Quake 2 names: no `^` codes, no `&c` codes.
  - A character with the high bit set is drawn in the alternate (green) half of the character set, as QuakeWorld draws its "red text".
  - Server-side copies and logs show the plain letter.
- **QuakeWorld, for comparison**: the name is drawn with high-bit "red" glyphs the same way. ezQuake's `&cRGB` colour codes are a client-side drawing feature (its login indicator default uses one). Whether they appear in names is not settled here. [read for the indicator; the rest open]

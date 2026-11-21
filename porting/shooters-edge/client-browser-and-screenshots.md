# The server browser's and screenshot settings (clean-room note)

WORKING AREA: deleted when porting ends. M18k4 round 30, client settings (ezQuake 3.6.9). Written by a clean-room reader in its own words, with no code; each fact is marked [read] or [derived].

Settings' names, defaults and one-line meanings are in client-inventory.md § 1.15 and § 1.16; match_auto_sshot's trigger conditions are in client-demo-settings.md. This note holds the details. The numbered sections answer the round's questions by number.
Names are only those a player, a server admin or a config file sees.

Background used by several answers [read]: the browser keeps one list of servers built from
the ticked ("marked") sources. A refresh has up to three phases: the sources update (asking
masters, reading files and URLs), the ping phase, and the info phase (one status query per
server). Each server holds a ping that is either a number of milliseconds or "none" (shown as
"n/a"). Filtering and sorting are recomputed only when the list is marked for a re-sort: at the
end of every refresh, when the server tab is shown, when any of sb_hideempty, sb_hidenotempty,
sb_hidefull, sb_hidedead, sb_hidehighping, sb_pinglimit, sb_showproxies or sb_info_filter is
changed, when the sort is changed from the keyboard or mouse, and when the search text changes.

## Hiding servers

### 1. sb_hidedead
- A server is dead when its ping is "none" [read]. It becomes "none" in any of these cases:
  no ping reply arrived during the ping phase, no status reply arrived during the info phase,
  or the status reply did not start with the expected header (four 0xFF bytes and the letter
  n) [read]. So either failure is enough: no ping, or no status [derived].
- A server just added to the list (from a master or a file) starts with ping "none" [read].
- During the info phase, servers already dead are not queried at all, so an info-only refresh
  never revives a dead server; only a full refresh (which pings again) can [read/derived].
- With sb_hidehighping on, servers above sb_pinglimit are not status-queried either, and at the
  end of the info phase every server that got no reply is set to "none". So a server skipped for
  its high ping ends the refresh as dead and shows "n/a" if it is shown at all [derived].
- While a refresh runs, a modal box covers the list ("Pinging Servers" or "Getting Infos",
  "Updating Sources" while masters are asked) with a progress bar; the list underneath keeps the
  filtering of the last re-sort and is re-filtered when the refresh ends [read]. If a re-sort is
  forced mid-refresh (by changing a hide setting), servers not yet answered count as dead at that
  moment [derived].
- Sorting always puts dead servers after live ones, whether or not they are hidden [read].
- If sb_hidedead is 0 and every server is dead when a refresh is asked for, the refresh is made a
  full one (sources, pings, infos) [read].

### 2. sb_hideempty and sb_hidenotempty
- Both count players plus spectators: a server is empty when the two together are zero [read].
- A status-reply player line whose ping is positive counts as a player; a ping of zero or below
  counts as a spectator (servers report spectators with a negated ping) [read].
- The client has no notion of a bot: a bot the server lists with a positive ping counts as a
  player, with a zero or negative ping as a spectator [derived]. Whether a server lists bots at
  all is up to the server; the source I read does not settle it.
- Neither setting applies to proxies (a server whose game directory shows "qizmo", or whose
  "*version" info starts with "qwfwd"); proxies are governed by sb_showproxies only [read].
- Both on together: every ordinary server is hidden (each is either empty or not), so only
  proxies remain, and only when sb_showproxies lets them through [derived].
- A server that never answered the status query has no players, so it counts as empty [derived].

### 3. sb_hidefull
- Full means: number of players (spectators NOT counted) is at or above the server's
  "maxclients" info value [read]. A server with no "maxclients" key is treated as having 255
  slots, so it is effectively never full [read/derived]. Spectator slots ("maxspectators") play
  no part [read].

### 4. sb_hidehighping with sb_pinglimit
- Hidden when the ping is strictly above the limit; a ping equal to the limit is kept [read].
- The ping compared is the measured ping, not the route-finding best ping [read].
- A server with ping "none" is never above the limit, so this setting does not hide it; only
  sb_hidedead does [derived]. (See answer 1 for the side effect that turns high-ping servers into
  dead ones at the end of a refresh.)
- sb_pinglimit is read as a whole number (default 80) [read].

### 5. sb_info_filter
- Syntax [read]: the value is split into words like a console command line (whitespace
  separates words; double quotes keep a word with spaces together). Each word must start with
  "+" (show) or "-" (hide), followed by a key name, optionally followed by "=" and a pattern.
  A word that does not start with + or - or has an empty key prints
  "Invalid info-filter: bad syntax (<word>)" to the console and is ignored. A pattern that is
  not a valid regular expression prints "Invalid rule definition: <reason>" and the word is
  ignored. These messages appear on every re-sort while the bad word stays in the setting
  [derived].
- The key is compared exactly, case-sensitively, with the info keys of the server's status
  reply (the server's info string carried in that reply: hostname, map, maxclients, *gamedir,
  and so on). It is not compared with anything else [read].
- The pattern is a Perl-compatible regular expression, matched case-insensitively and
  unanchored (it matches anywhere in the value unless the player writes ^ or $) [read].
- No pattern, an empty pattern after "=", or a pattern starting with "*" all mean "the key is
  present with a non-empty value" [read]. So "+map=*dm" is the same as "+map", not a wildcard
  match for "dm" [derived].
- The key "*" alone ("+*" or "-*") means "every server that reaches this word": it decides the
  rest at once [read].
- Evaluation [read]: words are tried left to right. A word whose key the server lacks is skipped
  (neither shows nor hides). The first word whose key is present and whose pattern matches
  decides: + shows the server, - hides it. If no word decides, the server gets the opposite of
  the sign of the last valid word in the setting; with no valid word at all, it is shown.
  Consequence: a server lacking the key is hidden under a lone "+key=…" and shown under a lone
  "-key=…" [derived].
- Worked examples [derived]:
  - "+map=dm": servers whose map name contains "dm" in any case are shown; all others,
   including servers without a map key, are hidden.
  - "-*gamedir=fortress": servers whose game directory contains "fortress" are hidden; all others
   are shown, including those without that key.
  - "+map=^dm6$ +map=^e1m2$": shown if the map is exactly dm6 or exactly e1m2; hidden otherwise.
  - "-*version=^qwfwd +*": servers whose version starts with qwfwd are hidden; every other
   server is shown (the "+*" word decides for them).
  - "+teamplay=^0$ -*": servers with teamplay exactly 0 are shown, every other server is hidden
   (the same as "+teamplay=^0$" alone).
  - "-ktxver -*": servers with a non-empty ktxver are hidden, and "-*" hides the rest too, so the
   list is empty.
- sb_info_filter is applied after the search text, sb_showproxies, sb_hidedead, the
  empty/not-empty test, sb_hidehighping and sb_hidefull; a server must pass all of them [read].

## The list's columns and order

### 6. Columns
- Left to right, with header words and widths in characters [read]:
  1. name ("name"): always shown; takes all the width the other columns leave.
  2. address ("address"), 21, sb_showaddress.
  3. ping ("png"), 3, sb_showping.
  4. game directory ("gamedir"), 8, sb_showgamedir.
  5. map ("map"), 8, sb_showmap.
  6. players ("plyrs"), 5, sb_showplayers.
  7. fraglimit ("fl"), 2, sb_showfraglimit.
  8. timelimit ("tl"), 2, sb_showtimelimit.
- One blank character separates neighbouring columns [read]. Each value is drawn centred in its
  column [read].
- Columns are laid out from the right edge leftward (timelimit first, address last). A column is
  left out when placing it would leave the name 5 characters or fewer; so on a narrow screen
  the address goes first, then the ping, and so on [read/derived].
- What each prints [read]:
  - name: the "hostname" info value, at most 60 characters, cut to the room left; if the server
   has no name yet, its address. A qwfwd proxy's name is prefixed with "proxy ".
  - address: dotted IPv4 and port, as "a.b.c.d:port"; a server entered without a port gets
   27500.
  - ping: "n/a", or the ping right-aligned in 3 digits, capped at 999. When route finding is on
   and a best route is known, the route's ping is shown instead.
  - gamedir: the first 8 characters of the "*gamedir" info value; if there is none and "*progs"
   is "666", the word "qizmo"; otherwise blank.
  - map: the first 8 characters of the "map" info value.
  - players: the player count (spectators excluded, capped at 99), a slash, and "maxclients"
   left-aligned in 2 characters; a missing or longer-than-2-character maxclients shows "99".
   Examples: " 3/16", "12/8 " [derived].
  - fl and tl: the "fraglimit" and "timelimit" values right-aligned in 2 characters; a value
   longer than 2 characters shows as "99" [read/derived].
- Colours [read]: the players text is blue for an empty server, green for one with players,
  red for a full one. The ping text is graded by how many whole frames of a 77 Hz tick
  (1000/77 ms, about 12.99 ms) it spans: under about 13 ms bright green; 13 to 26 green; 26 to
  39 yellow-green; 39 to 52 yellow; 52 to 65 orange; 65 to about 130 red-orange; 130 and above
  red [derived from the frame rule]. A "n/a" ping falls in the first band and is drawn bright
  green [derived].
- Row backgrounds [read]: the selected row is grey with a flashing arrow at the name column's
  end; qizmo rows dark blue; qwfwd proxy rows dark green, brighter green when that proxy is in
  the player's proxy chain; other rows alternate two greys.
- The header row highlights the column under the mouse; clicking a header changes the sort
  (answer 7). Ctrl and a digit 2 to 8 toggles the show setting of the column with that number
  (1, the name, cannot be hidden) [read].

### 7. sb_sortservers and sb_sortplayers
- Encoding [read]: the value is read one character at a time, left to right. Each digit is a
  column code; the first code whose comparison differs decides, the next ones break ties. A "-"
  turns on descending order for that code AND every code after it (it is never turned off
  again within the string) [read/derived]. The end of the string, or any character that is
  not a known code, ends the comparison with an arbitrary but fixed order [read].
- Before the string is consulted, two rules always apply [read]: servers with ping "none" go
  after servers with a ping, and servers that fail the filters go after those that pass.
- Server column codes [read]: 1 name (compared case-insensitively, with the game's coloured
  and gold characters folded to plain ones); 2 address (IP bytes, then port); 3 ping (the
  route's best ping when route finding is on and known); 4 game directory; 5 map; 6 player count
  (spectators excluded); 7 fraglimit; 8 timelimit. Text columns 4, 5, 7 and 8 compare
  case-insensitively as text, so fraglimit "10" sorts before "5" [derived].
- An empty sb_sortservers uses a built-in order [read]: servers with players but not full
  first, then full ones, then empty ones; within the first and last groups by ping ascending,
  within the full group by player count descending.
- Changing it with keys [read]: on the server tab, Alt and a digit 1 to 8; or a click on a
  column header (codes as above). If the string already starts with that digit (with or without
  a leading "-"), the leading "-" is toggled; otherwise the digit is put in front of the whole
  string, so earlier keys stay as tie-breakers. The string holds at most 31 characters
  [derived from its buffer].
- Default "32" decoded: by ping ascending, ties by address ascending [derived].
- sb_sortplayers sorts the all-players tab. Codes 1 to 8 are the same server columns, taken from
  the server each player is on (code 3 is the plain measured ping), and 9 is the player's own
  name (folded as for code 1) [read]. Players on dead servers always go last [read]. In this
  list a "-" has no effect: descending is switched off again before every comparison [derived].
  Keys: on the players tab, the plain digits 1 to 9 (no modifier, since Alt starts a name
  search there), with the same toggle-or-prepend rule [read].
- Default "92" decoded: by player name, ties by the server's address [derived].

### 8. sb_sortsources
- Same encoding and same "-" behaviour as the server sort [read]. Codes: 1 the source's type
  (master, then file, then url), 2 its name (folded as above), 3 the order in which the source
  was created (read from the sources file or added) [read].
- The first source, the built-in "Unbound" list that holds servers added by hand, always stays
  at the top and is not sorted [read]. When a server's details are open, sources never updated
  go last [read].
- Keys: the plain digits 1 to 3 on the sources tab, toggle-or-prepend as above [read].
- Default "3" decoded: in the order the sources were defined [derived].

### 9. sb_status
- With sb_status 1 the list loses three rows at the bottom: a divider and two status lines
  [read].
- The divider is a line across the width with "<n>/<total>" near its right end in the
  highlight colour, where n is the selected row's position (from 1) and total the number of
  servers that passed the filters [read].
- Line 1: the selected server's name (if it has at least 2 characters, otherwise its address),
  centred and cut to the width [read].
- Line 2, when no search is in progress, centred, fields separated by a centred dot [read]:
  "<gamedir> · <map> · <players>/<maxclients>-<spectators>/<maxspectators>" where gamedir and map
  are each at most 8 characters and appear only when the server has those keys, the counts are
  the full player and spectator counts, and a missing maximum shows "??". Then, if the server
  has a "status" info key: " · <status value>". Otherwise up to three more fields, each only
  when the key exists and it still fits the width: " · dmm<deathmatch>", " · fl:<fraglimit>",
  " · tl:<timelimit>" [read].
- Line 2 while a name search is in progress: "search for: <typed text>" (padded to at least 7
  characters), drawn in the highlight characters [read].

## Refreshing and querying

### 10. sb_pings, sb_pingtimeout, sb_pingspersec, sb_nosockraw
- The ping is a separate packet from the status query [read]: the client sends four 0xFF bytes,
  the letter k and a newline (6 bytes) to the server's game port and waits for a packet whose
  first byte is the letter l. The status query is a different packet (answer 11).
- In 3.6.9 the list refresh always pings this way over UDP: the client switches to the UDP
  method once at start-up, and sb_nosockraw is registered but never consulted, so it has no
  effect [read/derived]. The older ICMP echo path still exists in the code but is unreachable
  [derived].
- Refresh ping [read]: each distinct address and port is pinged once per round, all servers in
  turn, for sb_pings rounds (clamped to 1 to 6). Between two sends the client sleeps
  1000 / sb_pingspersec milliseconds, with the division truncated to a whole number (default
  150 gives 6 ms, about 167 pings a second) [read/derived]. Replies are collected by a second
  thread; after the last send it waits a fixed 500 ms more and stops [read]. sb_pingtimeout is
  not used by the refresh ping [read].
- The ping shown is the arithmetic mean of the round trips that came back, in whole
  milliseconds (truncated), capped at 999 for display; with no reply the ping is "none" [read].
  Replies are paired with sends in arrival order (the k-th reply is timed against the k-th
  send), so a lost reply makes later samples look longer [derived].
- A single-server ping (used when a server is added by hand with addserver or the Insert key)
  sends sb_pings pings (clamped to 1 to 10) one at a time, waiting up to sb_pingtimeout ms for
  each reply; the result is the mean of the replies minus 1 ms, or "none" [read].
- Servers listed in several sources are pinged once [read].

### 11. sb_inforetries, sb_infotimeout, sb_infospersec
- The status query is four 0xFF bytes followed by "status 23" and a newline (14 bytes) [read].
  The expected reply is four 0xFF bytes and the letter n, then the server's info string, a
  newline, and one line per client with: id, frags, time, ping, quoted name, quoted skin, top
  colour, bottom colour, quoted team [read]. At most 100 info keys and 128 clients are kept
  [read]. Spectator names in the form \s\name or name(s) are shown without the marker [read].
  Players are kept sorted by frags, highest first; info keys alphabetically, case-insensitively
  [read].
- Units: sb_infotimeout in milliseconds, sb_infospersec in queries per second [read].
- Pacing [read]: one query at most every 1/sb_infospersec seconds (default 100, so every 10
  ms). At each slot the client picks, among servers still owed a query whose previous query
  went out at least sb_infotimeout ms ago, the one with the fewest queries sent so far.
- Retries [read/derived]: sb_inforetries is the total number of queries per server, not the
  number after the first (default 3 means at most 3 packets). Each retry waits the full timeout
  after that server's previous query. Any reply from the server's address and port ends its
  queries.
- The phase ends when no server can be queried and 1.2 x sb_infotimeout has passed since the
  last query went out [read]. Servers that never answered get ping "none" [read].
- Not queried at all: dead servers, and, with sb_hidehighping on, servers above sb_pinglimit
  [read].
- A single-server status query (the r key on the server list, the live update, addserver) sends
  one packet and waits up to 1.5 x sb_infotimeout [read].
- The progress bar during this phase is the share of servers finished [read].

### 12. sb_masterretries, sb_mastertimeout
- The request to a master is 3 bytes: the letter c, a newline and a zero byte [read]. The
  reply must start with four 0xFF bytes, the letter d and a newline; then 6 bytes per server:
  4 IP bytes and a 2-byte port, high byte first [read]. Duplicate addresses are dropped; a reply
  can come in several packets and all of them are merged [read].
- "Retries" are not spaced: sb_masterretries copies of the request are sent back to back at
  once [read]. Then the client listens for sb_mastertimeout ms in total (milliseconds), taking
  every packet from that master's address and port in that window [read].
- Masters are asked one after another, so a sources update costs about sb_mastertimeout ms per
  master asked [derived].
- A master that returns no servers keeps its previous list [read]. A master's list replaces the
  old one only when at least one server came back [read].
- Updating one source on its own (the u key on the sources tab, or adding a source) behaves the
  same, except that each wait inside the window is up to 1.5 x sb_mastertimeout and a reply with
  the wrong header ends that source's update at once [read].
- File sources are read from the browser's folder (one address per line; a line without a port
  gets 27000) and URL sources are fetched over HTTP, following redirects, one address per line
  [read].

### 13. sb_liveupdate
- While a server's details window is open (opened with Enter or a click on a server, from the
  server list or the players list, or by addserver), a background loop wakes every second
  [read]. When sb_liveupdate is above 0, at least that many whole seconds have passed since the
  last update, and the menu has the keyboard, it re-sends one status query to the server whose
  details are shown and replaces its info and player list with the answer [read].
- The ping is not re-measured by the live update [read].
- Closing the details window stops the loop [read]. Switching to another server inside the
  window moves the updates to that server [read].
- 0 (or any value below 1, since only the whole part counts) turns it off [read/derived].
  Updates can never be faster than once a second [read].

### 14. sb_autoupdate
- Marked sources are the sources ticked on the sources tab (Enter, a click or ] toggles one,
  + or = ticks all, - unticks all, * inverts) [read]. The ticks are saved in the config as
  "sb_sourceunmarkall" followed by one "sb_sourcemark <name>" line per ticked source [read].
  At start the sources named "id limbo", "Global", "QuakeServers.net" and "Asgaard" are ticked
  if they exist; the config then applies its own lines [read]. If a refresh starts with nothing
  ticked, those defaults are ticked again [read].
- With sb_autoupdate 1, the first time the server list tab is shown in a run of the program, a
  full refresh starts: the marked sources are updated (every marked master asked again,
  regardless of sb_sourcevalidity), then the pings, then the infos [read/derived]. It happens
  once per run [read].
- For comparison [read]: Space on the server list does an info-only refresh (no sources update,
  no pings) unless no server passed the filters, in which case it is full; Ctrl+Space always does
  a full refresh. The console command "sb_refresh" is full; "sb_refresh info" is info-only.
  A refresh asked for while one is running prints "Server list refresh is still pending".
  At the end of a refresh the trigger "f_sbrefreshdone" fires; at the end of a sources update,
  "f_sbupdatesourcesdone".
- Esc or Backspace during a refresh cancels it (the box shows "cancelled"); the backquote,
  tilde and m keys leave the menus while it keeps running [read].

### 15. sb_autohide
- It acts when the player joins or observes from the browser (the j or p key to join, o or s
  to observe, and the same actions from a server's details window, including the direct
  connect that skips route finding) [read].
- 0: the browser stays open [read].
- 1: the menu closes and the game takes the keyboard, unless the chosen server's game directory
  column reads "qizmo" (a qizmo proxy), in which case the browser stays open [read].
- 2: the menu always closes [read].

### 16. sb_mastercache, sb_listcache, sb_sourcevalidity
- sb_mastercache 1: after a master answers with at least one server, its list is written to the
  browser's cache folder, "sb/cache" inside the "ezquake" folder of the install, under the name
  "<a>_<b>_<c>_<d>_[<port>].txt" built from the master's address; the text holds one
  "a.b.c.d:port" line per server [read]. A file source is rewritten in that same form in
  "sb/<its file name>" after each update that found servers when sb_mastercache is 1, and
  otherwise whenever one of its lines does not begin with a digit from 1 to 8 [read/derived].
- At start-up the cache is read back for every master and URL source, whatever sb_mastercache
  is, and the cache file's modification time becomes that source's last-update time [read].
- URL sources always cache their downloaded body in the same folder, under a name made from the
  URL: letters, digits and dots kept, every other character written as "_" and two lower-case
  hex digits [read].
- sb_sourcevalidity (minutes): when a sources update is NOT full, a master updated less than
  that many minutes ago is skipped [read]. The only non-full update is Space without Ctrl on the
  sources tab (and "sb_sourcesupdate" is full); every full refresh of the server list asks all
  marked masters again [read/derived]. The age is computed from day of month, hour, minute and
  second only, so a last update in an earlier month, or a clock that went back, counts as stale
  [derived].
- sb_listcache 1: after every refresh (full or info-only) the whole server list is written to a
  file named "servers_data" in the player's home folder for the game, and "Wrote server list
  contents to disk" is printed; the proxy list goes to "proxies_data" beside it [read]. On
  failure: "Couldn't write <path>." [read].
- Its format [read]: binary, the program's own memory layout: a 32-bit version number (1003),
  the size of one server record as the build sees it, the server count, the count that passed
  the filters, then each server record as raw bytes with its info keys left out. It is
  therefore only readable by the same build on the same platform [derived].
- It is read back once, at start-up, when the browser registers its settings with sb_listcache
  already 1 (the config has run by then) [read/derived]. Messages: "Successfully read <n>
  servers", "Format didn't match" (version or record size differ), "Format error (servers
  number too big)" (more than 1000), "Corrupted data", "No servers read", "Couldn't read
  <path>." [read]. Its age is never checked [read]. Servers read this way keep their ping,
  displayed texts and player counts, with blank player entries and no info keys, so
  sb_info_filter finds no keys on them until they are queried again [read/derived].

### 17. Other things the settings change that a player sees
- The search text: typing a letter or digit on the server list (without Ctrl or Alt) starts a
  name search; the list then shows only servers whose name contains the text, in any case.
  Backspace clears it [read].
- With no server passing the filters, the list shows "No servers filtered",
  "Press [space] to refresh the list", "Mark some sources on the next tab" and
  "or press [Insert] to add a server" [read].
- sb_showproxies 0 hides qizmo and qwfwd proxies, 1 shows them, 2 shows only them [read].
- Under a server's details window, a help box lists the keys ("j join", "o observe",
  "q QuakeTV obs.", "c copy to clipboard", "v say in chat", "ctrl+c copy addr.",
  "ctrl+v say team"), and with sb_findroutes on a route line: "Route: No route found",
  "Route: Direct route is best", "Route: 1 hop" or "Route: <n> hops" followed by "n direct
  connect" [read]. The box is left out on screens smaller than 384 by 216 [read].
- No password marker, lock symbol or colour for passworded servers exists in the browser
  [read: nothing in the browser looks at a password key].
- addserver <address> adds a server to the "Unbound" source (at most 200), pings and queries
  it and opens its details; errors: "Usage: addserver <addr>",
  "Error: maximum unbound servers number reached", "Error: couldn't resolve" [read].

## Screenshots

### 18. sshot_format
- Recognised values, case-insensitive: png (default), jpg, jpeg, tga, pcx [read]. Only PNG,
  JPEG and TGA files are ever written [read].
- When the screenshot command gets no name, the file's extension comes from the setting: png,
  jpg (for jpg or jpeg), tga or pcx; any other value gives png [read].
- How the file type is then chosen from the name [read]: a name ending in .tga, .png or .jpg
  (any case) is written in that type; a name ending .apng is written as PNG; otherwise the
  setting decides: png gives PNG, jpg or jpeg gives JPEG, anything else (pcx and unknown values
  included) gives TGA.
- The command's own argument therefore overrides the setting when it carries one of those
  three extensions [read]. The type's extension (lower case) is then appended unless the name
  already ends with exactly it [read]. Results [derived]: "shot" with png gives shot.png,
  "shot.tga" gives shot.tga in TGA; "shot.TGA" is TGA but becomes shot.TGA.tga; "shot.bmp" with
  png gives shot.bmp.png; "shot.jpeg" with png gives shot.jpeg.png.
- sshot_format pcx with no name: the name is made with .pcx, the content is TGA and the file
  ends up as "<name>.pcx.tga"; the counter checks for "<name>.pcx", which never exists, so
  every such screenshot overwrites ezquake000.pcx.tga [derived].
- The pixels have the hardware gamma ramp applied when hardware gamma is on, so the picture
  matches what the player saw [read].
- Messages: "Wrote <name>" or "Couldn't write <name>", where name is the given or generated name
  without the folder and before the extension is forced [read]. More than one argument prints
  "Usage: screenshot [filename]" [read].

### 19. sshot_dir
- Empty (the default): the current game folder (for example the base game folder or the mod's)
  [read].
- Not empty, by the separate setting cl_mediaroot (default 0) [read]: 0, relative to the
  folder the game is installed in, with leading slashes and backslashes removed; 1, relative to
  the player's home folder for the game, leading slashes removed; 2, taken as a full path.
- Folders are created when the first write fails: the writer makes the whole path and tries
  once more [read].
- No character is refused by the screenshot code: neither the folder nor a name given to the
  command is checked, apart from leading slashes and backslashes being stripped from the name
  [read]. So ".." in either reaches outside the folder [derived].

### 20. The default file name
- "ezquake" followed by a counter of at least 3 digits, zero-padded, then the extension: for
  example ezquake000.png, ezquake001.png [read].
- With sshot_autoname exactly 1: "<map name>_" followed by the counter, for example
  dm6_000.png [read].
- The counter is the first number from 0 upward whose file does not exist in the screenshot
  folder [read]. Past 999 it grows to 4 and 5 digits [derived]. The limit is 65535 names; at the
  limit: "Error: Cannot create more than 65535 screenshots" and nothing is taken [read].

### 21. image_jpeg_quality_level and image_png_compression_level
- JPEG quality (default 75): clamped to 0 to 100 [read]. PNG compression level (default 1):
  clamped to 0 (none) to 9 (best) [read]. Values are used as whole numbers, fractions dropped
  toward zero [derived]. Out-of-range values are clamped, never refused, and the setting keeps
  the value typed [read/derived].
- The PNG level also applies to animated PNG demo captures [read].

### 22. match_auto_sshot's file and timing
- Name [read]: the match name, the same one the automatic demo recording uses (built from the
  match_format_* pattern for the match type), followed by "_" and a 3-digit number and the
  extension. The match name is cleaned: control characters and ? * : < > " | become "_", runs of
  them collapse to one, while spaces and slashes are kept, so a pattern beginning "duel/" puts
  the picture in a "duel" subfolder [read/derived].
- The number is the first from 000 to 999 for which no file with that base name exists with any
  of the extensions pcx, tga, png or jpg [read]. With none free: "Error: no available
  filenames" [read].
- The type differs from the command's rule [read]: png if sshot_format is png, JPEG if jpg or
  jpeg, otherwise TGA (pcx and unknown values give TGA here, not PNG), unless the match name
  itself ends in one of the recognised extensions.
- Folder: the screenshot folder of answer 19 [read].
- Timing [read]: when the match ends and the client is in the ordinary intermission (not the
  end-of-episode finale), a countdown of 2 + gl_triplebuffer screen updates starts (default
  gl_triplebuffer 1, so 3 frames). While it runs the console and menus are not drawn and the
  scoreboard is drawn even if the console is down. At the end of the last of those frames,
  after post-processing, the screen is captured, provided the intermission is still on,
  and otherwise nothing is taken. Success prints "Match scoreboard saved to <file name>" [read].

### 23. scr_allowsnap
- How a server asks [read]: the server's admin command "snap <userid>" (or "snapall" for every
  non-spectator) makes the server send the client, reliably, the stuffed text "cmd snap". The
  client's forwarding command treats "snap" specially instead of passing it on.
- Nothing happens when the client is already uploading, is not yet on the server, or is playing
  a demo [read].
- With 1 [read]: the client grabs the screen, scales it to 800 by 600, writes it as a JPEG of
  quality 70 (TGA if the build has no JPEG) to a temporary file "temp/__rsshot__" under the
  screenshot folder, and uploads it to the server in chunks of at most 1350 bytes, each chunk
  carrying its byte count and the percentage done (0 to 100); the server asks for each next
  chunk with the stuffed command "nextul" and can stop it with "stopul". The client prints
  "Upload starting of <bytes>..." and, at the end, "Upload completed". The temporary file is
  deleted.
- With 0 [read]: the client sends the string command "snap" back to the server and prints
  nothing. A server of the same family answers by broadcasting "<name> refused remote
  screenshot" to everyone.
- The setting cannot be changed by a command the server stuffs while the client is connected
  (such a change is cancelled) [read]. The rulesets smackdown and smackdrive lock it at 1
  [read].
- On the server side the picture is stored as "snap/<userid>-<nn>.pcx" in the game folder with
  nn from 00 to 99 (the name says pcx whatever the content); with none free the admin sees
  "Snap: Couldn't create a file, clean some out." [read].

### 24. Other screenshot settings and commands
- The only still-picture command is "screenshot [name]" [read]. There is no console-only
  screenshot and no multiple-shot command in this release [read].
- sshot_autoname, default 0 (answer 20) [read].
- Demo capture ("demo_capture") writes its frames in the sshot_format type: tga, jpeg, jpg or
  png as named; "apng" gives one animated PNG file; any other value gives TGA [read]. Its
  folder is demo_capture_dir (default "capture") and its frame rate demo_capture_fps (default
  30) [read]. (Demo capture itself belongs to the demo settings note.)

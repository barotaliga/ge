# The classic packet forwarder: clean-room facts

A WORKING note of clean-room facts about the classic packet forwarder (the community's "qwfwd"
proxy that players chain between their client and a protocol 28 server), kept for the engine's
own forwarder ("forward", generic for every game, with a classic-compatible face for Shooter's
Edge). A reader session read QW-Group/qwfwd at master (2026-11-04, version 1.40-dev) with its
history, and the client's and the server's sides of chaining in QW-Group/ezquake-source and
QW-Group/mvdsv at master (2026-11-11), and wrote these facts in its own words with no code
(`clean-room` skill). Each fact is marked **read** (the source does it), **derived** (follows
from read facts, shown) or **measured** (none yet: no capture has been run; `se-interop`'s
loopback captures are the check). The classic handshake, info strings and the `pext` and real
address steps are specified in `protocol.md` (§ 2.2, § 5); this note adds what a forwarder
changes. "OOB header" means the four bytes FF FF FF FF that open a connectionless packet.

## 1. What it is and who runs it

1. **read** It is a small standalone UDP program that sits between a game client and a game
   server. The client connects to the forwarder as if it were a server; the forwarder opens its
   own connection to the real server (or to the next forwarder) and from then on copies the
   game's datagrams both ways unchanged. It speaks the classic protocol 28 and, in a reduced
   way, the later sequel's (Quake 3 style) protocol 68 handshake.
2. **read** It holds no game state and does not parse the game stream: it cannot show real pings,
   frags or skins (its status answer fakes them, Q6).
3. **derived** Why players use it: to replace a bad internet path with a better one. The client
   picks a forwarder whose own path to the server is good (a well-peered host near the
   server, or on a better backbone), so the sum client to forwarder plus forwarder to server can be
   lower or steadier than the direct route. The client's route finder (Q7) exists exactly to pick
   the chain with the lowest summed ping. Hiding the server's address is NOT a goal: the client
   names the server in its userinfo, and the server learns the client's real address anyway (Q5).
4. **read** Who runs it: community operators on hosts around the world (the client's own
   comment, written in 2011, counts about ten active ones). It announces itself to the same master
   servers game servers use, so it shows up in a server browser like a server, with
   `*version` starting "qwfwd" (that prefix is how the client tells a forwarder from a server).
5. **read** Default listening port: UDP 30000 on all addresses. One process, one socket for
   clients, plus one extra UDP socket per connected client towards the upstream (Q4).
6. **read** History of placement: originally (2008) it could also be loaded inside the server
   process as a shared library on its own port (`sv_qwfwd_port`, default 30000, 0 off). The
   current server has no trace of that; today it is a standalone binary only.
7. **read** It also answers STUN Binding requests (RFC 8489) on its client port, so a web page
   can list it as an ICE server and time the server-reflexive candidate to measure the round
   trip to the forwarder (browsers cannot send raw UDP). No game connection is made by that.

## 2. The operator's settings and commands

Settings (all **read**; "serverinfo" means mirrored into the info string the status reply sends):

| Setting | Default | Meaning |
| --- | --- | --- |
| `hostname` | "unnamed qwfwd" (the sample config sets "QWFWD") | name in the browser; serverinfo; also used in the "full" refusal |
| `maxclients` | 128 | most simultaneous clients; serverinfo |
| `hostport` | empty | the public "host:port" to advertise; serverinfo |
| `countrycode`, `city`, `coords` | empty | location shown to browsers (sample: "se", "Stockholm", "59.3327,18.0656"); serverinfo |
| `*version` | "qwfwd 1.40-dev" | read-only, serverinfo |
| `net_ip` | "0.0.0.0" (all addresses) | address to bind; settable only before start-up ends |
| `net_port` | 30000 | UDP port to bind; settable only before start-up ends |
| `developer` | 0 | 1 prints debug lines; 2 or more adds per-packet detail |
| `masters` | "master.quake.world master.quakeservers.net" | space-separated master list, each "host[:port]", default port 27000, at most 8 |
| `masters_heartbeat` | 1 | send heartbeats to the masters |
| `masters_query` | 1 | fetch the server list from the masters, ping those servers, and fill `pingstatus` |
| `masters_filter_servers` | "127.0.0.1" | up to 16 hosts (port ignored) never kept in the server list |

1. **read** A settable-once setting changed after start-up prints "<name> is write protected.".
   Changing a serverinfo setting updates the info string at once (nothing is pushed to
   connected clients).
2. **read** Command line: `qwfwd [port [ip]]`; a port or address given there overrides the
   config file's. Any of -h, -?, /?, /h, /help, -help, --help prints "Usage: <program> [port
   [ip]]". Further `+command` arguments run after start-up. At start it prints a banner and then
   "qwfwd: ready to rock at <ip>:<port>".
3. **read** Config files: `qwfwd.cfg` runs at start-up (before the network opens), then
   `qwfwd_listip.cfg` (the ban list). `exec` needs a .cfg name, refuses absolute paths, and
   looks in the folder `qwfwd/`, then `id1/`, then the working directory, then `qw/`. On SIGHUP
   (not Windows) it empties the whitelist and runs `qwfwd.cfg` again. `writeip` writes
   `qwfwd_listip.cfg` into the working directory.
4. **read** The console reads standard input only when both stdin and stdout are a terminal; to
   run in the background the output is redirected.

Commands (all **read**):

| Command | What it does |
| --- | --- |
| `quit` | clean exit at the end of the frame; `quit` with any argument exits at once |
| `serverinfo [key [value]]` | prints the info string with "[used/size]", one key, or sets one; keys starting `*` refuse with "Star variables cannot be changed." |
| `cllist` | a "client list" heading between rows of equals signs, then user id, address from, address to, minutes, name; then "<n> clients" |
| `svlist` | a "server list" heading between rows of equals signs, then index, address, ping in ms (65535 = no answer yet); then "<n> servers" |
| `heartbeat` | sends a heartbeat to every master now |
| `whitelist` | prints "whitelist: <n> addresses" and each |
| `whitelistadd <ip>` | adds a destination IP (at most 4096); errors "error: invalid IP address …", "error: … has already been added to the whitelist", "error: whitelist is full" |
| `whitelistremove <ip>` | removes one ("… removed from whitelist" or "error: … not found in whitelist") |
| `whitelistpurge` | empties the whitelist (so every destination is allowed again) |
| `addip <ip> [ban or safe] [time]` | adds a filter; octets left out or written 0 match anything ("192.246.40" is a /24); time is absolute Unix seconds, or "+N" for N seconds from now, or absent for permanent; all-zero refused "Bad filter address: …"; at most 1024 filters ("IP filter list is full") |
| `removeip <ip>` | removes the filter written the same way ("Removed." or "Didn't find …") |
| `listip` | "Filter list:" with each entry, its kind and seconds left |
| `writeip` | writes "addip a.b.c.d safe T" lines then "addip a.b.c.d ban T" lines to `qwfwd_listip.cfg` |
| `banip <ip> <N><s, m, h or d>` | timed ban, N clamped to 0..999; prints "… was banned for …", then adds the filter and runs `writeip`; refused when a `safe` entry has the same mask ("ban: can't ban such ip: …") |
| `banremove <id>` | removes ban number id (not a `safe` one) and runs `writeip` |
| `banlist` | "Ban list:" table (id, ip mask, type, expiry or "permanent"), safe entries first |

1. **read** Plus the generic console: `exec`, `echo`, `alias`, `unalias`, `unaliasall`, `wait`,
   `cmdlist`, `help`, `if`, `set`, `toggle`, `inc`, `cvarlist`, `cvar_hash_print`.
2. **read** Expired timed filters are removed every frame. A `safe` entry does not let anyone in; it only blocks `banip` of the same mask.
3. **read** History: a `password` setting (the client's `password` userinfo checked at connect)
   was merged and reverted the same day in February 2026; master has none.

## 3. The route in the userinfo and the connect, hop by hop

1. **read** The userinfo key is `prx`. Its value is the rest of the route after the forwarder
   that receives it: "server" for one forwarder, "fwd2[:port]@server[:port]" for two, and so
   on: hops separated by "@", each "host" or "host:port", the real server last.
2. **read** Client side (the modern client): the player sets `cl_proxyaddr` to the forwarder
   chain, "fwd1:port" or "fwd1:port@fwd2:port@…", and types `connect <server>` as usual. The client
   sends its packets to the first entry and sets `prx` to the remaining entries followed by "@"
   and the server ("fwd2:port@server" or just "server"). An empty `cl_proxyaddr` connects
   directly. Without client support a player can do the same by hand: `setinfo prx <server>`
   then `connect <forwarder>` (the forwarder's own 2010 notes give exactly this recipe for the
   sequel's client).
3. **read** `reconnect` after a connection through a forwarder reconnects to the last element
   of `prx` (the server) through the current `cl_proxyaddr`.
4. **read** Each hop pops itself: if the value holds an "@" with something after it, the part
   before the first "@" is this hop's destination and the part after becomes the new `prx`; otherwise the whole value is the destination and `prx` is removed from the userinfo. A
   destination without ":port" gets 27500 (classic) or 27960 (sequel). A port below 1 is
   refused with "port number in prx userinfo key is invalid". Host names are resolved by DNS
   at connect time (IPv4 only).
5. **derived** Because the default destination port is 27500, every forwarder in a chain after
   the first must be written with its port (they listen on 30000 by default).
6. **read** `prx` may be longer than the usual 63-character limit of an info value: the
   forwarder reads up to 255 characters of it and re-writes it without the length check (the
   client also sets it without the check). Other info limits stay: 1023 bytes for the whole
   userinfo, no backslash or double quote in a key or value.
7. **read** Every hop adds the star key `*qwfwd` with its short version ("1.40-dev"), so the
   server sees that the client came through a forwarder. No other key is added; nothing about
   the client's address is added.
8. **read** The forwarder's own checks on a classic `connect`, in order, each refused with an
   OOB print (byte `n`, a newline, the text): no challenge for this address and port, "No
   challenge for address."; version not 28, "Server is version 2.40."; challenge wrong, "Bad
   challenge."; malformed userinfo, "Invalid userinfo. Restart your qwcl"; too many clients,
   "proxy@<hostname> is full" followed by a blank line; no `prx`, "prx userinfo key is not
   set"; bad port (above). A destination that does not resolve, is not on a non-empty
   whitelist, is banned, or when no socket can be opened, is dropped silently (debug line only).
9. **read** The forwarder's challenge table holds 1024 entries keyed by full address including the
   port (the oldest is overwritten); the challenge is a random number from two calls of the C
   library's generator.

Packet sequence through ONE forwarder F to server S (client C), all **read** except where marked:

1. C to F: OOB "getchallenge" newline. (Sent as "getchallenge" with no newline it is taken as the
   sequel's protocol.)
2. F to C: OOB `c`, the challenge in decimal, a NUL. No extension pairs are offered.
3. C to F: OOB "connect 28 <qport> <challenge> "<userinfo with prx = S>"". **derived** Any extension
   lines after the first line are ignored: F reads only the first line's arguments.
4. F checks (above), pops `prx`, adds `*qwfwd`, records the client, and at once answers C with OOB
   `j` (5 bytes, no NUL), BEFORE the server has accepted anything.
5. F opens a fresh UDP socket for this client (unbound: the system picks the port) and from it
   sends S an OOB "getchallenge" newline, repeating it while unanswered every 3 seconds
   (**derived**: the check is "more than 2 whole seconds").
6. S to F's client socket: OOB `c` and its challenge (and its extension pairs, which F ignores: it
   keeps the leading decimal number only).
7. F to S: OOB "connect 28 <the client's qport> <S's challenge> "<rewritten userinfo>"" and a
   newline. No extension lines.
8. S to F: OOB `j`. F marks the client connected and does not pass the `j` on (C already had one).
9. From then on game datagrams flow both ways unchanged. **derived** The client began sending its
   reliable "new" right after step 4; F drops client datagrams until step 8, and the client's
   reliable resend delivers it afterwards.
10. A refusal print from S (OOB `n`, e.g. "server is full") is passed through to C unchanged, so the
   player sees why.

Through TWO forwarders F1 then F2 (C has `cl_proxyaddr` = "F1:p1@F2:p2"):

1. C runs steps 1 to 4 with F1, `prx` = "F2:p2@S".
2. F1 pops: destination F2:p2, new `prx` = "S"; adds `*qwfwd`; answers C with `j`.
3. F1, acting as the client, runs steps 5 to 8 against F2: getchallenge, `c`, then connect carrying
   C's qport and the userinfo with `prx` = "S". F2 checks it, pops (removes `prx`), sets `*qwfwd`
   again (same value), and answers F1 with `j` at once; F1 marks itself connected to F2.
4. F2 runs steps 5 to 8 against S with a userinfo that has no `prx` and has `*qwfwd`.
5. Game datagrams then travel C, F1, F2, S unchanged. **derived** Each forwarder only knows its
   neighbour on each side; S sees F2's address and per-client port.

The sequel's (protocol 68) variant, **read**: the client's getchallenge has no newline; the
answer is "challengeResponse <n>"; the client's connect is Huffman-compressed after its first
12 bytes and carries the userinfo with `challenge`, `qport` and `protocol` keys; the forwarder
decompresses it, re-adds its own `challenge` key upstream and re-compresses. It answers the
client "connectResponse" only once upstream is connected and the challenges agree; otherwise it
prints "/reconnect ASAP!", and the player must type `reconnect` by hand. A refusal is "print"
then the text. Chaining was noted as unsupported for the sequel when it was added.

## 4. Forwarding, timeouts, limits and abuse

1. **read** Datagrams are copied opaquely, byte for byte, one datagram out for one in. Client to
   upstream only once the hop is connected; upstream to client only when the sender's address and
   port equal the upstream's exactly, and only once connected. Everything from other senders on a
   client's upstream socket is dropped.
2. **read** Mapping: a client is known by its exact source IP and port (a linear search of a list).
   Each client gets its own upstream UDP socket, so the upstream sees the forwarder's IP and a
   distinct ephemeral port per client. The qport is passed upstream in the connect but not used
   by the forwarder to match packets. **derived** A client whose NAT changes its port mid-game
   becomes unknown to the forwarder (its packets are dropped) and times out; the forwarder does not
   use the qport to follow it.
3. **read** A new `connect` from an already known client address reuses its record and its upstream
   socket (so the upstream port stays the same), keeps its user id, may point it at a different
   destination, and restarts the challenge step.
4. **read** Receive buffer 8192 bytes: a datagram of 8192 bytes or more is dropped as
   "Oversize packet". Out-of-band packets the forwarder writes are at most 1450 bytes plus the
   header.
5. **read** Timeout: a client is dropped when no datagram has come FROM THE CLIENT for 15 seconds
   (whole-second clock, so 15 to 16 s). Upstream traffic does not keep it alive. The drop frees the
   record and closes its upstream socket at once; nothing is sent to either side.
6. **read** Disconnect detection: a classic client datagram whose byte at offset 10 (after the
   4-byte sequence, the 4-byte acknowledge and the 2-byte qport) is 4 (a string command) followed
   by the string "drop" marks the client for removal; that datagram is sent upstream 3 times (to
   survive loss), then the record is freed in the same frame.
7. **read** For the sequel's clients, once a client has been silent more than 1 second the forwarder
   sends the upstream a 6-byte packet (a zero sequence and the client's qport) every 50 ms, so the
   server answers "disconnect" if it has dropped the client; a "disconnect" from upstream is passed
   to the client and drops the record.
8. **read** Limits: `maxclients` (default 128) records in all, counting records still in the
   challenge step. **derived** The loop waits with select() on every socket, so on most systems the
   practical ceiling is the descriptor-set size (commonly 1024 descriptors).
9. **read** No rate limiting of any kind: no bandwidth cap, no packet-rate cap, no per-address
   connect limit, no cap on status or pingstatus answers.
10. **read** Which servers it may reach: any IPv4 host and port a client names, unless the
   destination whitelist (`whitelistadd`) is non-empty, in which case only listed destination IPs
   (any port). The ban filters apply to: every datagram arriving on the client port (by sender),
   every datagram arriving on an upstream socket (by sender), and the destination at connect.
11. **derived** Abuse surface: the forwarder only relays game datagrams after the destination
   answered `c` and `j`, so it cannot be used to stream arbitrary payloads to an arbitrary port; but anyone can make it send "getchallenge" every 3 s to any host and port for as long as they
   keep sending it packets. It is an amplifier for spoofed queries: a 15-byte `pingstatus`
   request returns up to 4101 bytes (about 270 times), a short `status` up to 8192 bytes, and a
   `getchallenge` returns a challenge; none of these is rate-limited or source-validated.

## 5. The game stream untouched; the client's real address

1. **read** After the handshake it never rewrites or adds a header: the bytes the client sends are
   the bytes the server gets, and the reverse. The only thing it reads is the "drop" test (Q4.6).
2. **read** Connectionless packets from a connected client are NOT forwarded except `rcon`: `ping`,
   `status`, `pingstatus`, `getchallenge` and `connect` are answered by the forwarder itself; any
   other text is dropped. Connectionless packets from the upstream: `c` and `j` are consumed, `n`
   prints are passed to the client, a bare disconnect byte (2) and `B` are consumed, anything else
   dropped.
3. **derived** Protocol extensions: the forwarder offers none in its `c`, strips the extension lines
   from the connect it forwards, and ignores the server's offer. They are recovered in-band: the
   modern server, for any client whose userinfo does not carry the `Qizmo` key or `*qtv`, stuffs
   "cmd pext" on `new`, and the client answers "pext" with its families and bits inside the
   game stream, which the forwarder passes untouched. The server's own source names this as the
   workaround for forwarders. `*qwfwd` does not disable it.
4. **read** The forwarder tells the server nothing about the client's address. The modern server
   finds it by asking the client: with `sv_getrealip` (default 1), on `new` it stuffs
   "packet <server ip:port> "ip <slot> <random>"" then "cmd new"; the client's `packet` command sends
   that text as an OOB packet DIRECTLY to the server, not through the forwarder; the server takes
   that packet's source as the client's real address if slot and random match and none is set yet,
   and drops the client if that address is banned. It tries up to 10 times or 3 seconds connected; with `sv_getrealip 2` a client that never proves its address is refused with "Failed to validate
   client's IP."; with 1 it is let in with the forwarder's address as its real one. `sv_serverip`
   names the server's public address when the server cannot work it out (it warns "Incorrect
   server ip address" and skips the check otherwise).
5. **read** The real address then serves the server's ban filters, VIP list, login by IP, the
   console's player listings and the mod's `realip` info key.
6. **derived** So a forwarder does not hide the client from the server, and a client that cannot
   reach the server directly (blocked path) fails the check, which matters only under
   `sv_getrealip 2`.

## 6. Its own packets: ping, status, masters, pingstatus, STUN

1. **read** Ping: an OOB packet whose text is "ping", or `k` followed by a NUL or newline, gets a
   single byte `l` (6C) with NO OOB header. Always answered, whatever `masters_query` is.
2. **read** Status: OOB "status [n]". The answer is the OOB header, `n`, then text. With n absent or
   0, or bit 1 set: the info string ("\key\value…") and a newline. With n absent or 0, or bit 2 or 4
   set: one line per client: "<userid> 0 <minutes> 666 "<name>" "" <top> <bottom>" and a newline.
   Frags are always 0, ping always 666, skin always empty; minutes count from the record's last
   challenge send, not truly from connect; name and colours (clamped 0..16) are taken from the
   connect userinfo and never updated. A reply that would exceed 8192 bytes is not sent.
3. **read** The info string carries `*version` ("qwfwd 1.40-dev"), `hostname`, `maxclients`, and
   `hostport`, `countrycode`, `city`, `coords` when set (an empty value is not stored).
4. **read** Heartbeat to each master, from the client port, every 300 s and at start-up: NO OOB
   header, the text "a", newline, a sequence number counting from 1, newline, the number of
   client records, newline (e.g. "a\n1\n0\n" written with real newlines).
5. **read** Master query, from the client port: the 3 bytes "c", newline, NUL, with no OOB header.
   Sent every 60 s until a master answers, then 1800 s after its last answer. The masters' names
   are re-resolved every 24 hours and whenever `masters` or `masters_query` changes.
6. **read** Master reply: OOB header, "d", newline (64 0A), then 6-byte entries: four IPv4 octets in
   order, then the port high byte first. Replies from an address not on the master list are
   refused ("Reply from not registered master server"). Up to 512 servers are kept; filtered hosts
   and duplicates are skipped.
7. **read** Server pinging: one probe every 0.1 s, round robin over the list, each server at most
   once per 60 s. Probe: OOB header, "k", newline (6 bytes). A 1-byte `l` arriving on the client
   port from a listed server sets its ping to the round trip in whole milliseconds. Until a first
   answer the ping is 65535. A server that has not answered and whose last probe is more than
   3600 s after its last answer is removed (a master may add it back). **derived** With 512
   servers a pass takes 51.2 s, so every server is probed about once a minute.
8. **read** `pingstatus` query: OOB header and the text "pingstatus" (15 bytes, no newline, as the
   client sends it). Answer: OOB header, `n`, then 8 bytes per server in list order: four IPv4
   octets in order, the port as a 16-bit value LOW byte first, the ping in ms as a 16-bit value low
   byte first (65535 = no answer yet). With `masters_query 0` the answer is the 5-byte header
   alone. **derived** At most 5 + 8 × 512 = 4101 bytes, one datagram (fragmented by IP above the
   path MTU).
9. **read** Note the two port orders: the master reply carries ports high byte first; the
   `pingstatus` answer low byte first (the 2009 history entry says the forwarder converts the port
   "to simplify parsing").
10. **read** STUN: a datagram of at least 20 bytes whose bytes 4..7 are the magic cookie 21 12 A4 42
   is taken as STUN and never forwarded. It is answered only if it is a Binding request (type
   0001), its length field equals the datagram length minus 20, and every attribute is
   comprehension-optional (type 8000 or above) with 4-byte padded lengths that end exactly at the
   datagram's end; the answer is 32 bytes: type 0101, length 12, the request's cookie and
   transaction id, and one XOR-MAPPED-ADDRESS (type 0020, length 8, family 01, port and IPv4
   XORed with the cookie). Anything else STUN-shaped gets no answer. (RFC 8489 defines these
   values.)
11. **read** It does not answer the master-style list or any other query; it does not report its
   clients' pings anywhere.

## 7. The client's route finder

1. **read** Driven by the client's server browser with `sb_findroutes 1` (default 0). After a
   refresh the client builds a "ping tree" (`sb_buildpingtree` builds it by hand):
2. **read** Nodes are IPv4 hosts. The player's machine links to every server the browser pinged,
   at the browser's measured ping (dead servers left out). Servers whose `*version` begins with
   "qwfwd" are forwarders; each is sent `pingstatus` (rate `sb_proxinfopersec`, default 10 per
   second; wait `sb_proxtimeout`, default 1000 ms; `sb_proxretries`, default 3 rounds) from its own
   socket, and its answer adds a link from that forwarder to every server it lists at the ping it
   reports (65535 entries skipped). A replying address must match the forwarder's IP.
3. **read** The shortest summed ping from the player to each server is computed (Dijkstra). The
   browser shows that best ping beside the direct one and can sort by it.
4. **read** Nodes are keyed by IP alone: a forwarder on the same IP as a server is the same node, and
   the port in a `pingstatus` entry is ignored.
5. **read** `connectbr <address>` and `observebr <address>` (the latter as spectator) print
   "Shortest path length: <n> ms" and the route hop by hop, then set `cl_proxyaddr` to the
   forwarder chain ("ip:port@ip:port", nearest first) and connect; they print "Connecting using
   <n> proxies with best ping <n> ms", or "Direct route is the best route, connecting
   directly...", or "No route found, trying to connect directly..." (both clear `cl_proxyaddr`).
   A missing port means 27500.
6. **read** Other knobs: `sb_ignore_proxy` (forwarders "ip:port" to leave out, matched as a
   substring), `sb_proxygetpings <ip:port>` (prints one forwarder's list), `sb_showproxies` (0 hides
   forwarders and Qizmo proxies from the list, 2 shows only them). With `sb_listcache` the answers
   are cached in a `proxies_data` file in the client's home folder and reloaded at start.
7. **derived** The route's cost is the sum of round trips per leg; it ignores the forwarders' own
   delay and load, and a forwarder only knows servers its masters list.

## 8. History, limits, and what the engine's forwarder should do better

1. **read** History (from the commit log): 2008 first version, inside the server as a library and
   standalone, with `status` and `ping`; 2009 `pingstatus`, `rcon` passed through, faster drop
   detection; 2010 console, settings, `qwfwd.cfg`, masters (query and heartbeat), `maxclients`,
   partial sequel support; 2011 `*qwfwd` key, extensions left to the server's in-band step, bans,
   `prx` allowed past 64 characters "for chaining"; 2011 127.0.0.1 filtered from master lists; 1.2 tag 2018; 1.30 release 2025 (more serverinfo keys, colour fix); 1.40-dev since: destination
   whitelist (2025), reload on SIGHUP (2025), password check added and reverted (2026), STUN
   answers (2026-11).
2. **read** Known limits: IPv4 only; no encryption or authentication; select()-based loop; peers
   in a linked list searched linearly; status shows fake pings and frags; `j` sent to the client
   before the server accepts; a client changing source port is lost; no rate limits; protocol
   extensions dropped from the handshake; sequel support needs a manual `reconnect`; time kept in
   whole seconds for timeouts.
3. **derived** What a modern rewrite should do better (our reading, not the source's):
   1. Hold the client's `j` until the upstream accepts (or relay the upstream's own `j` and its
      refusal), and relay the server's extension offer instead of dropping it, so the in-band
      `pext` step is not needed.
   2. Match a client by qport as well as address, so a NAT rebinding does not drop it.
   3. Rate-limit and size-cap answers to unauthenticated queries (status, pingstatus,
      getchallenge), and require a challenge before a large answer, to stop reflection.
   4. Restrict destinations by default (an allow list, or only servers from the masters' list),
      not open by default.
   5. Keep a client alive on traffic in either direction or with an explicit keepalive, measured
      on a sub-second clock.
   6. Pass the client's real address to the server in a way the server can trust (a key the next
      hop signs, or a forwarder list the server trusts) instead of a direct packet that fails when
      the direct path is blocked.
   7. IPv6; an event loop that scales past the select() limit; real per-client stats (bytes,
      loss, round trip) in status and in the console.
   8. Report pings in one documented byte order, and include per-route loss and jitter, not only
      the round trip.

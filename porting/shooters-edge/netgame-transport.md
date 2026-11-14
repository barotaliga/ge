# The single-player/netgame client's UDP connection (protocol 15): the transport and the signon (clean-room answers)

Reader's note for the clean room: what a server of our own (a TV that plays a recording) must do on
the wire for an unmodified 1996-97 single-player/netgame client (the original id release), and a
current client that keeps its protocol, to connect over UDP and watch as if it were a netgame
server: the datagram header and its flags, the connectionless control packets, the port the client
moves to, reliable and unreliable delivery, sizes, timeouts and keepalives, the signon as seen from
the server, the frame datagram, disconnecting, and the connect command. Facts only, in the reader's
words, each marked **[read]** (stated by the source), **[derived]** (worked out from read facts) or
**[open]** (the source does not settle it). Nothing was built, run or measured. It adds to
`protocol.md`, `demo-formats-single.md` and `demo-write-single.md` (the server messages and their
layouts) and does not repeat them.

## Sources read and their licences

| Source | Release read | Licence |
| --- | --- | --- |
| id Software's released source, its single-player/netgame tree (client and server, the Unix and Winsock UDP back ends) | github.com/id-Software/Quake at bf4ac42 (2012-01-31) | GPL v2 |
| QuakeSpasm (the current client checked) | quakespasm at e1c94a0 (2026-11-14) | GPL v2 |

A research session read these sources on 2026-11-14; the building session takes everything from this
note and never opens them. "The original client" means the id release; "QuakeSpasm" the current one.
Message ids are the protocol 15 numbers: server `nop` 1, `disconnect` 2, `time` 7, `print` 8,
`stufftext` 9, `serverinfo` 11, `signonnum` 25; client `nop` 1, `disconnect` 2, `move` 3,
`stringcmd` 4.
## 1. The datagram header

- Every datagram that is not a control packet starts with an 8-byte header of two 32-bit words, both
  **big-endian** (network order): word 0 is flags in the high 16 bits and the length in the low 16
  bits; word 1 is a sequence number. The payload follows directly. [read]
- **The length is the whole datagram's size in bytes, header included** (8 + payload). The mask for it
  is 0x0000FFFF. [read]
- Flag bits (exact values of word 0's high half, as a 32-bit word): DATA 0x00010000 (a reliable
  fragment), ACK 0x00020000 (acknowledges a reliable fragment), NAK 0x00040000, EOM 0x00080000 (set
  together with DATA on the last fragment of a reliable message), UNRELIABLE 0x00100000 (a whole
  unreliable message), CTL 0x80000000 (a control, "connectionless", packet). [read]
- NAK is defined but nothing in either source sends it or acts on it. [read]
- The kinds that occur: DATA (with or without EOM), ACK, UNRELIABLE, CTL. A DATA last-fragment word 0 is
  0x00090000 plus the length. An ACK is exactly 8 bytes: word 0 is 0x00020008, word 1 is the
  acknowledged fragment's sequence; no payload. [read]
- On a connected socket the receiver ignores datagrams shorter than 8 bytes and any datagram whose CTL
  bit is set. It takes the length from the header, not from the number of bytes received; the
  original never checks that they agree for DATA or UNRELIABLE packets, so a sender must make them
  equal. [read; the consequence derived]
- Precedence when several bits are set: UNRELIABLE is tested first, then ACK, then DATA. [read]

## 2. Control packets

- Layout: one 32-bit **big-endian** word, CTL (0x80000000) ORed with the whole datagram's length
  (this word included), then a one-byte command, then its body. There is **no sequence word**. [read]
- The receiver drops a control packet unless the flags part is exactly 0x80000000, the length equals
  the bytes received, and the word is not all ones. [read]
- Inside a control body, a "long" is 4 bytes **little-endian** (the game messages' byte order), a byte
  is 1 byte, a string is NUL-terminated. [read]
- The net protocol version is **3**; the game name is the string **"QUAKE"**. [read]

Requests (client to server), command byte then body: [read]

| Id | Request | Body |
| --- | --- | --- |
| 0x01 | connect | string "QUAKE", byte 3 |
| 0x02 | server info | string "QUAKE", byte 3 |
| 0x03 | player info | byte player index (counts only occupied slots, from 0) |
| 0x04 | rule info | string: the previous rule's name, empty for the first |

Replies (server to client): [read]

| Id | Reply | Body |
| --- | --- | --- |
| 0x81 | accept | long: the port number (an ordinary integer, e.g. 26001, not byte-swapped) |
| 0x82 | reject | string reason |
| 0x83 | server info | string server address "a.b.c.d:port", string host name, string level name, byte players now, byte max players, byte 3 |
| 0x84 | player info | byte index, string name, long colours (top in the high nibble of the low byte, bottom in the low nibble), long frags, long seconds connected, string the player's address |
| 0x85 | rule info | string name, string value; a reply with neither string ends the walk |

- The original server answers "QUAKE" with any other name by silence; a connect with a net protocol
  byte other than 3 gets reject "Incompatible version.\n"; no free slot gets reject
  "Server is full.\n"; a build with banning turned on can reject "You have been banned.\n". [read]
- A connect request from the same IP and port as a live connection made less than 2.0 s ago gets the
  same accept again (same port). From the same IP otherwise (later, or from another port) the original
  server closes the old connection and sends nothing; the client's next retry then gets a new slot.
  So the original server takes one connection per IP address. [read; the last sentence derived]
- The client sends the connect request up to **3 times**, waiting up to **2.5 s** after each (7.5 s in
  all), printing "trying..." first and "still trying..." after each silent wait. It then gives up with
  "No Response" (a socket error gives "Network Error", any reply byte other than 0x81 or 0x82 gives
  "Bad Response"), and the connect command ends with a console error ending "connect failed".
  A reject prints its reason; the menu keeps its first 31 characters. On accept it prints
  "Connection accepted". [read]
- A connect reply is believed only when it comes **from exactly the address and port the request was
  sent to**; anything else is ignored (QuakeSpasm prints "wrong reply address"). [read]
- Server info is what the LAN search sends: the client broadcasts a server-info request to the
  broadcast address on its own default port (26000 unless changed) and collects replies for 1.5 s, at
  most 8 servers. It files each server under the address in the reply's **server address string**,
  not the address the reply came from. A reply whose last byte is not 3 is listed with a "*" before
  its name. [read]
- Player info and rule info are only sent by two developer console commands ("test" and "test2"):
  connecting and browsing never send them. [read]
- **A plain connect to an address the client has not seen in a search first runs a silent LAN search**
  (the 1.5 s broadcast) before sending the connect request. Our server does not have to answer it.
  [read; the last sentence derived]

## 3. After the accept: the port

- The client stores the server's address as **the IP it sent the request to with the port from the
  accept**, and from then on sends every datagram there, from **the same local socket and port** it
  used for the request. [read]
- It then drops every datagram that does not come from exactly that IP and port (QuakeSpasm prints
  "Forged packet received" with both addresses). [read]
- The original server opens a new UDP socket on a port the system chooses for each client, names that
  port in the accept, and runs the whole connection on it. The listening port (26000 by default) only
  takes control packets; anything else arriving there is dropped. The per-client socket in turn drops
  control packets, and drops datagrams from any address but the client's IP and port as recorded from
  its connect request. [read]
- **The accept may name the listening port itself.** The client then simply keeps talking to the port
  it first used, which is the simplest thing behind NAT or a firewall; the server must then tell a
  client's control packets from its data on the one socket by the CTL bit and tell clients apart by
  source address and port. [derived]
- If the server does use a separate port per client, that port must be reachable from the client
  (forwarded through NAT, open in the firewall), because the client checks the source of every reply.
  [derived]
- The client can only reach IPv4 addresses (neither source has an IPv6 path). [read]

## 4. Reliable messages

- **One reliable message is in flight per direction at a time.** The sender cannot start the next
  message until every fragment of the current one is acknowledged. [read]
- **Fragments carry at most 1024 bytes of payload** in the original (1032-byte datagrams). A message of
  1024 bytes or fewer is one datagram with DATA and EOM; a longer one is sent as 1024-byte DATA
  fragments without EOM and a last one with EOM. [read]
- **Stop and wait:** the next fragment goes out only after the ACK for the previous one arrives (in the
  same receive pass, or at the next point the sender asks whether it can send). [read]
- **Sequence numbers count fragments, not messages.** The reliable send counter starts at **0** for
  each new connection and goes up by 1 per new fragment; each side keeps its own counter for each
  direction, and the reliable and unreliable counters are separate. [read]
- The receiver **acknowledges every DATA datagram it gets**, duplicates and out-of-order ones too, with
  an ACK carrying that datagram's sequence. It keeps the fragment only when the sequence equals the
  next one it expects; any other is counted as a duplicate and dropped. On EOM the reassembled message
  is handed to the game. [read]
- The sender takes an ACK only when its sequence is that of the **latest fragment sent** and that
  fragment has not been acknowledged yet; any other ACK is ignored ("stale" or "duplicate"). [read]
- **Resend:** while a fragment is unacknowledged, it is sent again, with the same sequence, whenever
  more than **1.0 s** has passed since the last send of it. The check runs once each time the
  connection is read, which is once per host frame. There is no limit on the number of resends.
  [read]
- An unacknowledged reliable never ends the connection by itself; only the overall timeout of § 7
  does, and that timer is refreshed by unreliable datagrams too. [derived]
- QuakeSpasm differs as a **sender**: its fragments may carry up to 64000 bytes. Its client's reliable
  messages are at most 1024 bytes (the client's reliable buffer), so in practice they still fit one
  datagram, but a server should accept a DATA datagram of any size up to 64008 bytes. As a receiver it
  takes 1024-byte fragments as the original does. [read; the advice derived]

## 5. Unreliable messages

- One unreliable message is one datagram with the UNRELIABLE flag; it is never split. Its sequence is a
  separate counter starting at **0** and rising by 1 per datagram. [read]
- The receiver keeps the next expected unreliable sequence. A datagram with a **lower** sequence
  (an old or duplicate one) is dropped, and the receiver **stops reading the socket for this frame**,
  and datagrams queued behind it wait for the next frame. A higher sequence is accepted; the gap is counted
  as dropped datagrams (shown by the "net_stats" command). Either way the expected number becomes the
  received one plus 1. [read]
- An unreliable datagram ends the read pass: at most one unreliable message is taken per read, and the
  client reads repeatedly in a frame until nothing is left. [read]

## 6. Maximum sizes

- Original client, unreliable: payload at most **1024 bytes** (a 1032-byte datagram). It reads at most
  1032 bytes per datagram. A larger datagram is cut short on Unix; on Windows the read fails and the
  client reports "Read error" and loses the connection. So never send the original client a datagram
  over 1032 bytes. [read; the Windows outcome derived from how the Winsock call treats a short buffer,
  not stated in the source]
- Original client, reliable: the reassembled message must fit **8192 bytes**; the server side of the
  original never builds one over **8000** bytes (its per-client reliable buffer). Past 8192 the client
  stops with a fatal error. Keep reliable messages at 8000 bytes or fewer. [read; 8000 as the safe
  limit derived]
- The original client's own unreliable `move` is 16 bytes of message (a 24-byte datagram); its reliable
  messages are at most 1024 bytes. [derived from the layout in § 9 and the buffer size read]
- QuakeSpasm accepts reliable and unreliable messages up to 64000 bytes, so whatever fits the original
  fits it. [read]
- The length field is 16 bits, so no datagram can declare more than 65535 bytes. [read]

## 7. Timeouts and keepalives

- **Timeout:** a connection whose last complete message (a whole reliable message or an unreliable
  datagram; ACKs do not count) arrived more than **net_messagetimeout** seconds ago is closed; the
  default is **300**. The same rule holds on the server side and in QuakeSpasm. The client then stops
  with a console error ending "lost server connection". [read]
- So a client facing a silent server waits 300 s; one unreliable datagram (even a lone `nop`) inside
  every 300 s keeps it alive. [derived]
- **While the client loads the level** (between taking `serverinfo` and sending "prespawn"; § 8 and
  § 12) it reads the connection after loading each model and each sound. Then: a reliable message
  arriving is a fatal error for the connection (a console error ending "received a message"), an
  unreliable datagram whose first byte is not `nop` (1) is too ("datagram wasn't a nop"), and a lone
  `nop` datagram is fine and prints the line "server to client keepalive" (after an arrow
  pointing left). At most once every 5 s it sends
  the server a **reliable** message holding one client `nop` (1) and prints the line
  "client to server keepalive" (after an arrow pointing right). [read]
- The original server, for a client that has not yet sent "begin" and has no signon reply pending,
  sends an unreliable datagram holding the single byte `nop` (1) when more than **5 s** have passed
  since it last sent that client anything. It is not required by the client (300 s is the limit),
  but matches what the client expects to see. [read; "not required" derived]
- **The idle client:** once fully connected it sends a `move` every client frame whatever the player
  does (§ 9), so the server always hears from it. Before that it sends only its reliable signon replies
  and the loading keepalives. [read]
- The client's `nop` datagram filter: an unreliable or reliable message that is exactly one byte, 1, is
  thrown away before parsing (and printed as the keepalive line above). [read]

## 8. The signon over the network

What the original server does, and what the client answers: [read]

1. **On accept** the server sends, as **one reliable message**: a `print` with byte 2 (the colour
   marker) and the line "VERSION 1.09 SERVER (n CRC)" (the version, 1.09 in the release
   read, and the game program's checksum),
   then `serverinfo` (protocol 15, max clients, game type, level name, model list, sound list), then
   `cdtrack`, `setview` (the client's own entity number, slot + 1), then `signonnum` **1**.
2. The client parses it, loads everything (§ 7), then answers with a reliable `stringcmd` **"prespawn"**.
3. The server answers "prespawn" with one reliable message: the level's signon buffer (baselines,
   static entities, static sounds) and `signonnum` **2**.
4. The client answers with one reliable message of three `stringcmd`s in this order:
   **"name \"<name>\"\n"**, **"color <top> <bottom>\n"**, **"spawn <parms>"** (the parms are usually
   empty, leaving "spawn " with a trailing space).
5. The server answers "spawn" with one reliable message: `time`, then for **every** slot `updatename`,
   `updatefrags`, `updatecolors`; then all 64 `lightstyle`s; four `updatestat`s (total secrets, total
   monsters, secrets found, monsters killed); `setangle` (pitch, yaw, roll 0); `clientdata`; and
   `signonnum` **3**.
6. The client answers with a reliable `stringcmd` **"begin"**. The server marks the client spawned and
   from its next frame sends it a frame datagram every frame (§ 9).
7. **Fully connected:** the client is at stage 3 until the first **fast update** (an entity update,
   first byte with bit 0x80 set) arrives in any message; that makes it stage 4, removes the loading
   plaque and starts rendering and sending `move`s. No `signonnum` 4 is sent. [read]

- The client checks only that each `signonnum` is **greater** than its current stage; an equal or lower
  one is a fatal error "Received signon n when at m". Its reply depends on the number it reaches
  (1 prespawn, 2 name/color/spawn, 3 begin, 4 nothing). [read]
- The server keeps each signon reply until the client asks for it, so the client's answer comes back
  before the next stage is sent; a server may answer each request as soon as it arrives. [read; the
  second half derived]
- The original server does not send the client frame datagrams before "begin". The client's
  `stringcmd`s it obeys from a remote player: status, god, notarget, fly, name, noclip, say, say_team,
  tell, color, kill, pause, spawn, begin, prespawn, kick, ping, give, ban; anything else is ignored.
  Any console command the viewer types that the client does not know is forwarded to the server as a
  `stringcmd`, so a server must expect and ignore arbitrary text. [read]
- **Level change:** the original server sends every client a reliable `stufftext` "reconnect\n"; the
  client drops back to stage 0 with the loading plaque, on the same connection. The server then sends a
  new `serverinfo` (step 1) and the signon repeats. [read]
- `serverinfo` resets the client's whole level state, so anything about the level must follow it.
  [read]

## 9. During play

The client's messages: [read]

- **`move` (3)**, unreliable, one per client frame once fully connected: float (4 bytes, little-endian)
  the time of the last `time` the client received (the server takes its ping from it); three angles,
  one byte each (degrees × 256 / 360), pitch, yaw, roll; three signed 16-bit little-endian shorts,
  forward, side and up speed; one byte of buttons (bit 0 attack, bit 1 jump); one byte impulse (0 for
  none). 16 bytes in all. The first two `move`s after each connect or level change are not sent.
- **`stringcmd` (4)**, reliable: a NUL-terminated string (§ 8).
- **`nop` (1)**, reliable, during loading only (§ 7).
- **`disconnect` (2)**, unreliable, when leaving (§ 11).
- Several client messages may share one datagram; the server reads them in order until the message
  ends. An unknown byte ends the connection on the original server. [read]

The server's frame message, sent to each spawned client every server frame as **one unreliable
datagram**: `time` (float, the server's time in seconds), the client's `clientdata`, an entity
update for each entity the client can see, then the frame's sounds, particles and other broadcast
events if they still fit in 1024 bytes. Reliable things (prints, frags, names, `stufftext`, level
change) go in the client's reliable message, sent as soon as the previous one is acknowledged. [read]

How the client uses `time`: [read]

- Each `time` shifts the previous value back and keeps the new one; the client draws entities
  between the last two. Its own clock advances by its frame time (0.001 to 0.1 s per frame); the draw
  point is placed between the two server times by that clock.
- If the two server times are more than **0.1 s** apart (a dropped datagram, or the start) the older is
  treated as 0.1 s before the newer. If the clock runs more than 0.01 of the gap behind the older time
  or past the newer one, it is snapped to that time (so the client follows the server's pace and does
  not drift).
- **An entity not included in the latest message is removed from the view** for that frame, so every
  frame datagram must carry every entity to be seen, not only those that changed.
- An entity that moves more than 100 units on any axis between two messages jumps instead of gliding.
- Two equal times, or "cl_nolerp" 1, mean no interpolation.
- Time going backwards is not refused; it produces a gap below zero and the clock is snapped to one of
  the two times, a visible jump. Keep `time` rising. [derived]
- In a network game the view angles are the viewer's own (mouse and keys); the server turns the view
  only with `setangle`. So a server that wants the viewer to see a recorded player's view must send
  `setangle` each frame; otherwise the viewer looks around freely from the `setview` entity's
  position. [read; the advice derived]

## 10. The server's frame rate

- The original host runs at most **72 frames a second** (a listen server, that is a game that also
  draws). A dedicated server runs one frame every **sys_ticrate** seconds, default **0.05** (20 a
  second). One frame datagram goes to each spawned client per server frame. [read]
- There is no rate setting or cap per client in the original: every frame, one datagram. [read]
- The client does not depend on a particular rate: any steady pace works with the interpolation in
  § 9; gaps over 0.1 s are drawn as 0.1 s steps. [derived]
- QuakeSpasm's frame cap is the "host_maxfps" setting, default 72. [read]

## 11. Disconnect

- **Client leaving:** it sends one **unreliable** datagram holding `disconnect` (2), not resent and
  not acknowledged, then closes its socket. No control packet. [read]
- **Server dropping a client:** it sends `disconnect` (2) in a **reliable** message (only when no
  other reliable is in flight), then closes the connection. When the whole server shuts down it sends
  `disconnect` to all clients as a reliable message and waits up to **5 s** for their ACKs. [read]
- **Client receiving `disconnect`:** it ends the game with "Server disconnected", and sends its own
  `disconnect` datagram as it closes. [read]
- Closing a connection sends nothing at the transport level; there is no close packet. [read]
- A client that just vanishes is noticed by the 300 s timeout (§ 7). [derived]

## 12. What a server must do while the client loads

- The client must have every model and sound in `serverinfo` (the map itself first) in its own game
  files. A missing model prints "Model ... not found" and the level is not loaded; there is no
  download. [read; the outcome after the message open]
- Between sending the `serverinfo` message and receiving "prespawn": **send no other reliable
  message, and no unreliable datagram except a lone `nop`.** Anything else ends the client's connection
  (§ 7). The client also cannot take frame datagrams until it has the level, and the original server
  sends none before "begin". [read]
- Acknowledge the client's reliable `nop` keepalives (a normal ACK) and ignore them. [read]
- A `nop` every 5 s while waiting is what the original server sends; it is optional within the 300 s
  limit. [read; "optional" derived]
- Answer "prespawn" and "spawn" promptly; the client sits on the loading plaque until the first entity
  update after `signonnum` 3. [read]
- Nothing else is needed: no console "connect" line or debug setting on the client. [derived]

## 13. QuakeSpasm, and the connect command

- **QuakeSpasm connects to a server that speaks only the original transport and protocol 15:** its
  connect request is the same ("QUAKE", 3), it reads accept and reject the same way, its header, flags,
  sequences, ACK rules, 1.0 s resend, 300 s timeout and loading keepalive rules are the same, and it
  accepts `serverinfo` protocol 15 (as well as its own 666 and 999). It sends `move` angles as one byte
  each to a protocol 15 server. Its signon replies are the same strings in the same order. [read]
- Its differences that matter to our server: it may send reliable fragments larger than 1024 bytes
  (§ 4), it prints "Forged packet received" and "wrong reply address" for datagrams from the wrong
  port, and it accepts larger messages. [read]
- **Default port: 26000.** The command-line option "-port n" (or "-udpport n") changes it, and so does
  the console command "port n" (1 to 65534). The client also sends to that port when the address names
  none. The original's TCP/IP join menu has a port field that sets it. [read]
- **The command:** "connect <address>". In QuakeSpasm, **connect 1.2.3.4:26001** works as typed. In
  the original client a ":" splits a console word, so the port must be quoted,
  **connect "1.2.3.4:26001"** (which is what the original's join menu itself types), or the port set
  first with "port 26001" and then "connect 1.2.3.4". [read]
- The port suffix works only on a numeric address. A host name always uses the default port, in both
  clients ("connect example.org" goes to port 26000 unless "port" was changed; "example.org:26001" does
  not resolve). A numeric address may be partial; the missing leading parts are taken from the
  client's own address. [read]
- The other client commands a viewer may use: "reconnect" (wait for the signon again on the same
  connection), "disconnect", "net_stats" (the transport's counters). [read]

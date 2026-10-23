# The classic protocol, specified

The multiplayer protocol, version 28, in our words (README.md, the rules). An outline to be
filled message by message with M18g, each one checked against loopback captures of existing
programs (`se-interop`).

## Connectionless packets (spec)

A packet starting with four 0xFF bytes is out of band: a text command follows.

- `status` (and its variants): the server answers with its server info and a player per line;
  this is what a server browser asks.
- `getchallenge` → the server answers `c` and a challenge number (and, on extended servers, the
  extension bits it offers: verify).
- `connect 28 <qport> <challenge> "<userinfo>"` → `j` on acceptance.
- Heartbeats to master servers, and a master's server list: format to be written from captures.

## The net channel (spec)

Every in-band packet starts with the outgoing sequence (u32; the top bit set when the packet
carries the reliable message) and the last sequence received (u32; the top bit the reliable
acknowledgement bit). A client then writes its qport (u16) so the server finds it after a NAT
changes its port. The reliable message is resent until acknowledged and only one is in flight.

## Messages

Server to client (svc) and client to server (clc) messages: a byte id, then its body. The
table (id, name, body, when sent) is written with M18g; the shapes to cover are the server
data, the model and sound lists, the baselines, the per-frame player states and packet entities
as deltas against the acknowledged frame, the client's three usercmds a packet with their
checksum, string commands, downloads, prints, sounds, light styles, stats, the intermission, and
the extension bits each one depends on.

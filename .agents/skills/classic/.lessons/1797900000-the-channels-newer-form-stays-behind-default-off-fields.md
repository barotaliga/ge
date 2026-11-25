---
title: The channel's newer form stays behind fields that default to the old one
date: 2026-11-21
scope: plugins/classic/protocol/channel
concepts: [classic, channel, qport, fragments, protocol-36, net]
---

Quake II's protocols 35 and 36 change the channel (a one-byte qport or none, 30-bit sequences, a
message in pieces) while Quake, QuakeWorld and protocol 34 keep the old one. Every new behaviour is a
`Channel` field whose default is the old form (`qport_bytes` 2, `fragments` false, `packet_len`
`kMaxPacket`), set only by the endpoint after it reads the connect's protocol; read packets with
`channel_read`, which hands back the payload once its pieces are whole. A client is then matched by
each protocol's qport form, not by a fixed two bytes. A test client of the later protocols must
receive a datagram after a loss before its ack can show the gap that makes the server resend.

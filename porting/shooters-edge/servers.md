# Servers: what maps onto the classic server and match mod (working note)

WORKING AREA: deleted when porting ends. The engine's design is `docs/servers.md` (M53); this
note only says which classic programs the later rows match in behaviour, so the names stay out of
the tree.

- **The classic server program** (MVDSV, GPL): the reference for M53f–M53g's feature list: rcon,
  the status queries, ban lists and filters, the multiview demo recording (`.mvd`) and its relay
  to watchers (the QTV relay), spectators, downloads, the extension set it advertises. Behaviour
  only, from its documentation and black-box runs; no source read by a building session.
- **The classic match mod** (KTX, GPL, shipped as a QC progs and as a native library): M53h runs
  its QC progs build unchanged on the QC VM and mod server. That needs the extension builtins
  it calls; the list is gathered from its documented requirements and from running it, not from
  its source. Its native library form is out of scope (it is the classic server's own ABI).
- **Our own match mod** reproduces the match tools' behaviour (ready, break, votes, timeouts, the
  match modes, stats) in our words, with the modern behaviour behind meta-cvars (M53e).

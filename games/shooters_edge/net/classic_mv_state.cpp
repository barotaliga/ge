// A match kept from its multiview stream and written as a late viewer's catch-up
// (classic_mv_state.hpp).
//
// ge:summary The multiview stream's state kept a record at a time; the late viewer's catch-up
// ge:keywords multiview stream-state catch-up mv_apply mv_catch_up keyed-messages entity-list
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_test.cpp
#include "shooters_edge/net/classic_mv_state.hpp"

#include <string>
#include <utility>

#include "shooters_edge/net/classic_delta.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_wire.hpp"

namespace shooters_edge::net {

namespace {

constexpr size_t kMostEntities = 2048;

// The recorder's player form: the origin's and the angles' bits, then the rest.
constexpr int kMvAngles = 8, kMvEffects = 64, kMvSkin = 128, kMvDead = 256, kMvGib = 512;
constexpr int kMvWeaponFrame = 1024, kMvModel = 2048;

uint32_t u32_at(std::span<const uint8_t> b, size_t at) {
  return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
         static_cast<uint32_t>(b[at + 2]) << 16 | static_cast<uint32_t>(b[at + 3]) << 24;
}

void put_u32(std::vector<uint8_t>& b, uint32_t v) {
  for (int i = 0; i < 4; i++) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

std::string two(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

// A scratch writer.
struct Scratch {
  std::vector<uint8_t> buf = std::vector<uint8_t>(size_t{1} << 16);
  gn::Writer w = gn::make_writer(buf.data(), buf.size());
  std::span<const uint8_t> bytes() const { return {buf.data(), w.overflow ? 0 : w.size}; }
};

// Read records of time 0 filled with whole messages, each cut under kMostCatchUpBody.
struct Records {
  std::vector<uint8_t>& out;
  std::vector<uint8_t> body;
  void add(std::span<const uint8_t> msg) {
    if (!body.empty() && body.size() + msg.size() > kMostCatchUpBody) flush();
    body.insert(body.end(), msg.begin(), msg.end());
  }
  void flush() {
    if (body.empty()) return;
    mv_put_record(out, 0, kMvRead, 0, 0, body);
    body.clear();
  }
};

void forget_level(MvState& s) {
  const int64_t records = s.records, levels = s.levels;
  std::string name = std::move(s.name);
  s = MvState{};
  s.records = records;
  s.levels = levels;
  s.name = std::move(name);
}

// A message as a relay passes it on: the server's name in the server info rewritten to the
// relay's own. True when `out` holds a changed message.
bool renamed(MvState& s, const SvcMsg& m, std::vector<uint8_t>& out) {
  std::string text;
  if (m.id == svc::serverinfo && m.strings.size() == 2 && m.strings[0] == "hostname") {
    const std::string name = mv_hostname(s.name, m.strings[1], s.upstream_tv);
    if (name.size() >= kServerInfoBytes) return false;
    out.push_back(svc::serverinfo);
    text = std::string("hostname") + '\0' + name;
  } else if (m.id == svc::stufftext && m.strings.size() == 1 &&
             m.strings[0].starts_with("fullserverinfo \"")) {
    const std::string& t = m.strings[0];
    const size_t open = t.find('"'), close = t.find('"', open + 1);
    if (close == std::string::npos) return false;
    std::string info = t.substr(open + 1, close - open - 1);
    s.upstream_tv = !info_get(info, "*qtv").empty();
    if (!info_set(info, "hostname", mv_hostname(s.name, info_get(info, "hostname"), s.upstream_tv),
                  kServerInfoBytes - 1) ||
        !info_set(info, "*qtv", "1", kServerInfoBytes - 1))
      return false;
    out.push_back(svc::stufftext);
    text = t.substr(0, open + 1) + info + t.substr(close);
  } else {
    return false;
  }
  out.insert(out.end(), text.begin(), text.end());
  out.push_back(0);
  return true;
}

bool server_data(MvState& s, gn::Reader& r) {
  int32_t v = read_long(r);
  ExtFamily f{};
  Ext ext;
  while (!r.bad && v != kProtocolVersion && family_of_magic(static_cast<uint32_t>(v), f)) {
    ext[f] = gn::read_u32(r);
    v = read_long(r);
  }
  read_long(r);    // the server's count of levels
  read_string(r);  // the game folder
  read_float(r);   // the server's time (a client's has its slot here)
  read_string(r);  // the level's name
  for (int i = 0; i < 10; i++) read_float(r);
  s.ext = ext;
  return !r.bad && v == kProtocolVersion;
}

void player(MvState& s, gn::Reader& r) {
  const int slot = read_byte(r);
  const int bits = read_short(r) & 0xFFFF;
  MvPlayer scratch;
  MvPlayer& p = slot >= 0 && slot < 32 ? s.players[slot] : scratch;
  const Coords c = coords_of(s.ext);
  p.anim = read_byte(r);
  float* o[3] = {&p.origin.x, &p.origin.y, &p.origin.z};
  float* a[3] = {&p.angles.x, &p.angles.y, &p.angles.z};
  for (int i = 0; i < 3; i++)
    if ((bits & (1 << i)) != 0) *o[i] = read_origin(r, c);
  for (int i = 0; i < 3; i++)
    if ((bits & (kMvAngles << i)) != 0) *a[i] = read_angle16(r);
  if ((bits & kMvModel) != 0) p.model = read_byte(r);
  if ((bits & kMvSkin) != 0) p.skin = read_byte(r);
  if ((bits & kMvEffects) != 0) p.effects = read_byte(r);
  if ((bits & kMvWeaponFrame) != 0) p.weaponframe = read_byte(r);
  p.dead = (bits & kMvDead) != 0;
  p.gib = (bits & kMvGib) != 0;
  p.frame = s.frames;
}

void write_player(gn::Writer& w, int slot, const MvPlayer& p, Coords c) {
  int bits = 1 | 2 | 4 | kMvAngles | kMvAngles << 1 | kMvAngles << 2 | kMvModel | kMvSkin |
             kMvEffects | kMvWeaponFrame;
  if (p.dead) bits |= kMvDead;
  if (p.gib) bits |= kMvGib;
  write_byte(w, svc::playerinfo);
  write_byte(w, slot);
  write_short(w, bits);
  write_byte(w, p.anim);
  write_origin(w, p.origin.x, c);
  write_origin(w, p.origin.y, c);
  write_origin(w, p.origin.z, c);
  write_angle16(w, p.angles.x);
  write_angle16(w, p.angles.y);
  write_angle16(w, p.angles.z);
  write_byte(w, p.model);
  write_byte(w, p.skin);
  write_byte(w, p.effects);
  write_byte(w, p.weaponframe);
}

void baseline(MvState& s, const EntityState& st) {
  if (st.number < 0 || static_cast<size_t>(st.number) >= kMostEntities) return;
  for (size_t i = s.baselines.size(); i <= static_cast<size_t>(st.number); i++) {
    s.baselines.emplace_back();
    s.baselines.back().number = static_cast<int>(i);
  }
  s.baselines[static_cast<size_t>(st.number)] = st;
}

// A message read whole: what it changes kept. `bytes` are its own.
void note(MvState& s, const SvcMsg& m, std::span<const uint8_t> bytes, int kind, int target) {
  const auto at = [&](size_t i) { return i < m.ints.size() ? m.ints[i] : 0; };
  if (m.id == svc::updatestat || m.id == svc::updatestatlong) {
    if ((kind != kMvStats && kind != kMvSingle) || target < 0 || target >= 32) return;
    const int i = at(0);
    if (i < 0 || i >= kStats) return;
    s.stats[target][i] = at(1);
    s.stats_known[target][i] = true;
    return;
  }
  if (kind != kMvRead && kind != kMvAll) return;
  std::string key;
  switch (m.id) {
    case svc::serverinfo:
      key = "0sv:" + (m.strings.empty() ? std::string() : m.strings[0]);
      break;
    case svc::lightstyle:
      key = "1ls" + two(at(0));
      break;
    case svc::updateuserinfo: {
      key = "2ui" + two(at(0));
      const std::string own = "3si" + two(at(0)) + ":";
      for (auto it = s.keyed.lower_bound(own); it != s.keyed.end() && it->first.starts_with(own);)
        it = s.keyed.erase(it);
      break;
    }
    case svc::setinfo:
      key = "3si" + two(at(0)) + ":" + (m.strings.empty() ? std::string() : m.strings[0]);
      break;
    case svc::updatefrags:
      key = "4fr" + two(at(0));
      break;
    case svc::updateping:
      key = "5pi" + two(at(0));
      break;
    case svc::updatepl:
      key = "6pl" + two(at(0));
      break;
    case svc::updateentertime:
      key = "7et" + two(at(0));
      break;
    case svc::cdtrack:
      key = "8cd";
      break;
    case svc::intermission:
      key = "8im";
      break;
    case svc::setpause:
      key = "9pa";
      break;
    case svc::spawnbaseline: {
      EntityState b;
      b.number = at(0);
      b.modelindex = at(1);
      b.frame = at(2);
      b.colormap = at(3);
      b.skin = at(4);
      if (m.floats.size() >= 6) {
        b.origin = {m.floats[0], m.floats[2], m.floats[4]};
        b.angles = {m.floats[1], m.floats[3], m.floats[5]};
      }
      baseline(s, b);
      return;
    }
    case svc::spawnbaseline2:
      if (!m.ents.empty()) {
        EntityState b = m.ents[0].state;
        b.number = at(0);
        baseline(s, b);
      }
      return;
    case svc::spawnstatic:
    case svc::spawnstatic2:
    case svc::spawnstaticsound:
    case svc::spawnstaticsound2:
      if (!s.signon) s.statics.insert(s.statics.end(), bytes.begin(), bytes.end());
      return;
    default:
      return;
  }
  s.keyed[key].assign(bytes.begin(), bytes.end());
}

// A record's messages walked into the state.
// `sent`, when not null, gathers the messages as a relay passes them on (`renamed`).
bool walk(MvState& s, std::span<const uint8_t> body, int kind, int target,
          std::vector<uint8_t>* sent) {
  size_t copied = 0;  // body bytes already in `sent`
  gn::Reader r = gn::make_reader(body.data(), body.size());
  WireCtx ctx;
  ctx.ext = s.ext;
  ctx.mvd = true;
  while (gn::remaining(r) > 0) {
    const size_t start = r.at;
    const uint8_t id = body[start];
    if (id == svc::serverdata) {
      r.at++;
      if (!server_data(s, r)) {
        s.why = "the server data was not protocol " + std::to_string(kProtocolVersion);
        return false;
      }
      ctx.ext = s.ext;
      continue;
    }
    if (id == svc::playerinfo) {
      r.at++;
      player(s, r);
      if (r.bad) {
        s.why = "a player's message ran past its record";
        return false;
      }
      continue;
    }
    if (id == svc::packetentities || id == svc::deltapacketentities) {
      r.at++;
      const bool delta = id == svc::deltapacketentities;
      if (delta) read_byte(r);
      std::vector<EntityState> out;
      if (!read_packet_entities(r, delta ? &s.ents : nullptr, s.baselines, s.ext, out)) {
        s.why = "an entity list did not read";
        return false;
      }
      s.ents = std::move(out);
      continue;
    }
    SvcMsg m;
    std::string error;
    if (!read_svc(r, m, ctx, error)) {
      s.why = error;
      return false;
    }
    std::span<const uint8_t> bytes = body.subspan(start, r.at - start);
    std::vector<uint8_t> changed;
    if (sent && renamed(s, m, changed)) {
      sent->insert(sent->end(), body.begin() + static_cast<std::ptrdiff_t>(copied),
                   body.begin() + static_cast<std::ptrdiff_t>(start));
      sent->insert(sent->end(), changed.begin(), changed.end());
      copied = r.at;
      bytes = changed;
    }
    note(s, m, bytes, kind, target);
  }
  if (sent && copied > 0)
    sent->insert(sent->end(), body.begin() + static_cast<std::ptrdiff_t>(copied), body.end());
  return true;
}

}  // namespace

MvSplit mv_record(std::span<const uint8_t> bytes, MvRecord& out) {
  out = MvRecord{};
  if (bytes.size() < 2) return MvSplit::more;
  out.ms = bytes[0];
  out.kind = bytes[1] & 7;
  out.target = bytes[1] >> 3;
  size_t at = 2;
  if (out.kind == kMvSet) {
    if (bytes.size() < at + 8) return MvSplit::more;
    out.body = at;
    out.size = 8;
    out.whole = at + 8;
    return MvSplit::ok;
  }
  if (out.kind < kMvRead || out.kind > kMvAll) return MvSplit::bad;
  if (out.kind == kMvMultiple) {
    if (bytes.size() < at + 4) return MvSplit::more;
    out.mask = u32_at(bytes, at);
    at += 4;
  }
  if (bytes.size() < at + 4) return MvSplit::more;
  const uint32_t len = u32_at(bytes, at);
  if (len > kMostRecordBody) return MvSplit::bad;
  out.body = at + 4;
  out.size = len;
  out.whole = out.body + len;
  return bytes.size() < out.whole ? MvSplit::more : MvSplit::ok;
}

void mv_put_record(std::vector<uint8_t>& out, int ms, int kind, int target, uint32_t mask,
                   std::span<const uint8_t> body) {
  out.push_back(static_cast<uint8_t>(ms));
  out.push_back(static_cast<uint8_t>(kind | target << 3));
  if (kind == kMvMultiple) put_u32(out, mask);
  put_u32(out, static_cast<uint32_t>(body.size()));
  out.insert(out.end(), body.begin(), body.end());
}

std::string mv_hostname(const std::string& own, const std::string& theirs, bool upstream_tv) {
  if (own.empty()) return theirs;
  std::string server = theirs;
  // A relay before this one named itself; the server's own name is the part it kept.
  const std::string live = " (live: ";
  const size_t at = server.find(live);
  if (upstream_tv && at != std::string::npos && server.ends_with(")"))
    server = server.substr(at + live.size(), server.size() - at - live.size() - 1);
  return server.empty() ? own : own + live + server + ")";
}

bool mv_apply(MvState& s, std::span<const uint8_t> record, std::vector<uint8_t>* sent) {
  MvRecord h;
  if (sent) sent->assign(record.begin(), record.end());
  if (mv_record(record, h) != MvSplit::ok) {
    s.broken = true;
    s.why = "a record was not whole";
    return false;
  }
  s.records++;
  const std::span<const uint8_t> body = record.subspan(h.body, h.size);
  const bool level =
      (h.kind == kMvRead || h.kind == kMvAll) && !body.empty() && body[0] == svc::serverdata;
  if (level) {
    forget_level(s);
    s.level = s.signon = true;
    s.levels++;
  }
  if (!s.level) return true;  // joined mid-level: nothing is known until the next one
  if (s.signon && h.ms != 0 && !level) s.signon = false;
  if (!s.signon && h.ms != 0) s.frames++;
  // A record to some players is their own business, but for the stats one alone is sent.
  const bool read =
      h.kind == kMvRead || h.kind == kMvAll || h.kind == kMvStats || h.kind == kMvSingle;
  bool ok = true;
  std::vector<uint8_t> passed;  // the body renamed (empty: as it came)
  if (!s.broken && read) {
    if (!walk(s, body, h.kind, h.target, s.name.empty() ? nullptr : &passed)) {
      s.broken = true;
      ok = false;
      passed.clear();
    }
  }
  std::vector<uint8_t> rewritten;
  std::span<const uint8_t> out = record.first(h.whole);
  if (!passed.empty() && passed.size() <= kMostRecordBody) {
    mv_put_record(rewritten, h.ms, h.kind, h.target, h.mask, passed);
    out = rewritten;
  }
  if (s.signon) {
    s.signon_records.insert(s.signon_records.end(), out.begin(), out.end());
    s.signon_records[s.signon_records.size() - out.size()] =
        0;  // its time: the catch-up is at once
  }
  if (sent) sent->assign(out.begin(), out.end());
  return ok;
}

std::vector<uint8_t> mv_catch_up(const MvState& s) {
  std::vector<uint8_t> out = s.signon_records;
  if (!s.level || s.signon || s.broken) return out;
  Records rec{out, {}};
  for (const auto& [key, bytes] : s.keyed) rec.add(bytes);
  {
    // The statics after the signon, message by message (each starts with its id; their bytes
    // were kept whole, so a reader walks them again to cut).
    gn::Reader r = gn::make_reader(s.statics.data(), s.statics.size());
    WireCtx ctx;
    ctx.ext = s.ext;
    ctx.mvd = true;
    while (gn::remaining(r) > 0) {
      const size_t start = r.at;
      SvcMsg m;
      std::string error;
      if (!read_svc(r, m, ctx, error)) break;
      rec.add(std::span<const uint8_t>(s.statics).subspan(start, r.at - start));
    }
  }
  const Coords c = coords_of(s.ext);
  for (int k = 0; k < 32; k++) {
    const MvPlayer& p = s.players[k];
    if (p.frame < 0 || p.frame + 1 < s.frames) continue;
    Scratch o;
    write_player(o.w, k, p, c);
    rec.add(o.bytes());
  }
  rec.flush();
  for (int k = 0; k < 32; k++) {
    Scratch o;
    for (int i = 0; i < kStats; i++) {
      if (!s.stats_known[k][i]) continue;
      SvcMsg m;
      const int32_t v = s.stats[k][i];
      m.id = v >= 0 && v <= 255 ? svc::updatestat : svc::updatestatlong;
      m.ints = {i, v};
      write_svc(o.w, m, WireCtx{});
    }
    if (o.w.size > 0) mv_put_record(out, 0, kMvStats, k, 0, o.bytes());
  }
  Scratch o;
  write_byte(o.w, svc::packetentities);
  write_packet_entities(o.w, nullptr, s.ents, s.baselines, s.ext);
  mv_put_record(out, 0, kMvRead, 0, 0, o.bytes());
  return out;
}

}  // namespace shooters_edge::net

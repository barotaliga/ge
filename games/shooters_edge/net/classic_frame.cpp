// The classic server's frame out (classic_server.hpp): what the mod wrote, routed to each client's
// reliable stream or this frame's datagram, and every client's packet: the choke count, the
// players it sees, the entities against its acknowledged frame, the stats that changed.
//
// WHO SEES WHAT: a player is sent when he is spawned, not a spectator, and in the client's
// visible set (every point is in every set until maps carry them); the client's own entry has no
// msec and no command (he predicts himself); a dead player's command has pitch 0. Entities: every
// entity past the clients with a model, sorted by number, at most 64 (256 under the extension),
// the nearest kept when there are more; numbers above 511 only under entity-double.
//
// ge:summary The classic server's frame out: events routed, players, entities, stats, the send
// ge:keywords route_events send_frames playerinfo packetentities delta-frame stats updatestat
// ge:keywords temp-entity sound multicast chokecount datagram visible-entities
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#include <algorithm>
#include <cstring>

#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/net/classic_server.hpp"

namespace shooters_edge::net {

namespace pg = ge::progs;

namespace {

WireCtx ctx_of(const ClassicClient& c) {
  WireCtx ctx;
  ctx.ext = c.ext;
  ctx.chunked = c.ext.has(ExtFamily::x, kXChunkedDl);
  return ctx;
}

bool wants(const sv::NetEvent& ev, int slot) { return (ev.to >> slot) & 1u; }

// True when a mod's raw bytes read as whole svc messages for this client. A mod that writes a
// malformed message (a temporary entity without its svc byte, a field cut short) would leave the
// client unable to read the rest of its packet, so such bytes are dropped and counted instead.
bool well_formed(const ClassicClient& c, const uint8_t* data, size_t size) {
  gn::Reader r = gn::make_reader(data, size);
  const WireCtx ctx = ctx_of(c);
  std::string error;
  while (remaining(r) > 0) {
    SvcMsg m;
    if (!read_svc(r, m, ctx, error)) return false;
  }
  return true;
}

// Appends a mod's raw bytes to the client's reliable stream or this frame's datagram when they are
// well formed and fit; otherwise counts them dropped.
void mod_bytes(ClassicServer& s, int slot, const uint8_t* data, size_t size, bool reliable) {
  ClassicClient& c = s.clients[slot];
  if (size == 0) return;
  if (!well_formed(c, data, size)) {
    s.counts.mod_bytes_dropped += static_cast<int64_t>(size);
    return;
  }
  if (reliable)
    reliable_bytes(s, slot, data, size);
  else if (c.datagram.size() + size <= kMaxMsgLen / 2)
    c.datagram.insert(c.datagram.end(), data, data + size);
  else
    s.counts.effects_dropped++;
}

void unreliable_svc(ClassicServer& s, int slot, const SvcMsg& m) {
  ClassicClient& c = s.clients[slot];
  uint8_t buf[kMaxMsgLen];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  write_svc(w, m, ctx_of(c));
  if (w.overflow || c.datagram.size() + w.size > kMaxMsgLen / 2) {
    s.counts.effects_dropped++;
    return;
  }
  c.datagram.insert(c.datagram.end(), buf, buf + w.size);
}

int sound_index(const ClassicServer& s, const std::string& name) {
  for (size_t i = 1; i < s.qc->sounds.size(); i++)
    if (s.qc->sounds[i] == name) return static_cast<int>(i);
  return 0;
}

float field(const ClassicServer& s, int e, int offset) {
  if (offset < 0) return 0;
  return pg::as_float(*pg::entity_field(s.qc->q.vm, e, offset));
}

int model_index_of(const ClassicServer& s, int32_t str) {
  const char* name = pg::vm_string(s.qc->q.vm, str);
  if (name == nullptr || name[0] == 0) return 0;
  for (size_t i = 1; i < s.qc->models.size(); i++)
    if (s.qc->models[i] == name) return static_cast<int>(i);
  return 0;
}

void route_event(ClassicServer& s, const sv::NetEvent& ev) {
  for (int j = 0; j < sh::kMaxClients; j++) {
    ClassicClient& c = s.clients[j];
    if (c.state != ConnState::spawned || !wants(ev, j)) continue;
    SvcMsg m;
    switch (ev.kind) {
      case sv::EventKind::print:
        client_print(s, j, ev.level, ev.text);
        break;
      case sv::EventKind::centerprint:
        m.id = svc::centerprint;
        m.strings = {ev.text};
        reliable_svc(s, j, m);
        break;
      case sv::EventKind::stuffcmd:
        stuff(s, j, ev.text);
        break;
      case sv::EventKind::lightstyle:
        m.id = svc::lightstyle;
        m.ints = {ev.level};
        m.strings = {ev.text};
        reliable_svc(s, j, m);
        break;
      case sv::EventKind::sound: {
        const int snd = sound_index(s, ev.text);
        const bool ext = snd > 255 || ev.ent > 1023;
        if (ext && !c.ext.has(ExtFamily::x, kXSoundDbl)) {
          s.counts.effects_dropped++;
          break;
        }
        m.id = ext ? svc::soundextended : svc::sound;
        m.ints = {ev.ent, ev.level, static_cast<int>(ev.values[0]),
                  static_cast<int>(ev.values[1] * 64), snd};
        m.floats = {ev.origin.x, ev.origin.y, ev.origin.z};
        if (ev.reliable)
          reliable_svc(s, j, m);
        else
          unreliable_svc(s, j, m);
        break;
      }
      case sv::EventKind::particle:
        m.id = svc::temp_entity;
        if (ev.text == "explosion") {
          m.ints = {te::explosion};
        } else if (ev.text == "blood") {
          m.ints = {te::blood, static_cast<int>(ev.values[1]) / 2};
        } else if (ev.text == "lightning blood") {
          m.ints = {te::lightningblood};
        } else {
          s.counts.effects_dropped++;
          break;
        }
        m.floats = {ev.origin.x, ev.origin.y, ev.origin.z};
        unreliable_svc(s, j, m);
        break;
      case sv::EventKind::ambient:
        m.id = svc::spawnstaticsound;
        m.floats = {ev.origin.x, ev.origin.y, ev.origin.z};
        m.ints = {sound_index(s, ev.text), static_cast<int>(ev.values[0]),
                  static_cast<int>(ev.values[1])};
        reliable_svc(s, j, m);
        break;
      case sv::EventKind::multicast:
        mod_bytes(s, j, ev.bytes.data(), ev.bytes.size(), ev.reliable);
        break;
      case sv::EventKind::pause:
        m.id = svc::setpause;
        m.ints = {s.qc->paused ? 1 : 0};
        reliable_svc(s, j, m);
        break;
      default:
        break;  // static entities are routed from the list; drops by classic_frame
    }
  }
}

PmType pm_of(const ClassicServer& s, int slot) {
  const sh::Edict& e = s.qc->sv.edicts[static_cast<size_t>(slot) + 1];
  switch (sh::move_type_of(e, false)) {
    case sh::MoveType::spectator:
      return PmType::spectator;
    case sh::MoveType::old_spectator:
      return PmType::old_spectator;
    case sh::MoveType::fly:
      return PmType::fly;
    case sh::MoveType::none:
      return PmType::none;
    case sh::MoveType::lock:
      return PmType::lock;
    default:
      return s.qc->sv.clients[static_cast<size_t>(slot)].jump_held ? PmType::normal_jump_held
                                                                   : PmType::normal;
  }
}

PlayerState player_state(const ClassicServer& s, int about, int to) {
  const sh::Server& sv = s.qc->sv;
  const sh::Edict& e = sv.edicts[static_cast<size_t>(about) + 1];
  const sh::ClientSlot& cs = sv.clients[static_cast<size_t>(about)];
  PlayerState p;
  p.slot = about;
  p.origin = e.origin;
  p.frame = static_cast<int>(field(s, about + 1, pg::kFFrame));
  if (about != to) {
    p.has_msec = true;
    const double ms = 1000 * (sv.time - cs.last_cmd_time);
    p.msec = ms > 255 ? 255 : ms < 0 ? 0 : static_cast<int>(ms);
    p.has_command = true;
    p.command = cs.last_cmd;
    p.command.buttons = 0;
    if (e.health <= 0) {
      p.command.angles = {0, e.angles.y, 0};
    }
  }
  p.velocity = {static_cast<float>(static_cast<int>(e.velocity.x)),
                static_cast<float>(static_cast<int>(e.velocity.y)),
                static_cast<float>(static_cast<int>(e.velocity.z))};
  const int model = static_cast<int>(field(s, about + 1, pg::kFModelindex));
  p.modelindex = model == s.player_model ? 0 : model;
  p.skin = static_cast<int>(field(s, about + 1, pg::kFSkin));
  p.effects = static_cast<int>(field(s, about + 1, pg::kFEffects));
  if (about == to && s.f_weaponframe >= 0) {
    p.has_weaponframe = true;
    p.weaponframe = static_cast<int>(field(s, about + 1, s.f_weaponframe));
  }
  p.dead = e.health <= 0;
  p.gib = e.mins.z != -24;
  p.pm = pm_of(s, about);
  p.onground = (e.flags & sh::kFlagOnGround) != 0;
  p.solid = e.solid != sh::kSolidNot;
  return p;
}

void update_stats(ClassicServer& s, int slot) {
  ClassicClient& c = s.clients[slot];
  const int e = slot + 1;
  int32_t st[kStats] = {};
  st[0] = static_cast<int32_t>(field(s, e, pg::kFHealth));
  st[2] =
      model_index_of(s, static_cast<int32_t>(*pg::entity_field(s.qc->q.vm, e, pg::kFWeaponmodel)));
  st[3] = static_cast<int32_t>(field(s, e, s.f_currentammo));
  st[4] = static_cast<int32_t>(field(s, e, s.f_armorvalue));
  for (int i = 0; i < 4; i++) st[6 + i] = static_cast<int32_t>(field(s, e, s.f_ammo[i]));
  st[10] = static_cast<int32_t>(field(s, e, s.f_weapon));
  const float serverflags = pg::as_float(s.qc->q.vm.globals[pg::kGServerflags]);
  st[15] =
      static_cast<int32_t>(field(s, e, pg::kFItems)) | (static_cast<int32_t>(serverflags) << 28);
  if (c.ext.has(ExtFamily::z, kZViewHeight))
    st[16] = static_cast<int32_t>(field(s, e, pg::kFViewOfs + 2));
  for (int i = 0; i < kStats; i++) {
    if (c.stats_known[i] && c.stats[i] == st[i]) continue;
    if (i == 16 && !c.ext.has(ExtFamily::z, kZViewHeight)) continue;
    SvcMsg m;
    m.id = st[i] >= 0 && st[i] <= 255 ? svc::updatestat : svc::updatestatlong;
    m.ints = {i, st[i]};
    reliable_svc(s, slot, m);
    c.stats[i] = st[i];
    c.stats_known[i] = true;
  }
}

// The visible entities for a client, sorted by number, on its grid.
void visible_entities(const ClassicServer& s, int slot, std::vector<EntityState>& out) {
  const ClassicClient& c = s.clients[slot];
  out.clear();
  const sh::Server& sv = s.qc->sv;
  const int limit = c.ext.has(ExtFamily::x, kX256Entities) ? 256 : 64;
  const int top = c.ext.has(ExtFamily::x, kXEntityDbl2)  ? 2048
                  : c.ext.has(ExtFamily::x, kXEntityDbl) ? 1024
                                                         : 512;
  const sh::Vec3f me = sv.edicts[static_cast<size_t>(slot) + 1].origin;
  for (int e = sv.set.maxclients + 1; e < sv.num_edicts && e < top; e++) {
    const sh::Edict& ed = sv.edicts[static_cast<size_t>(e)];
    if (ed.free) continue;
    const EntityState st = entity_state(s, e);
    if (st.modelindex == 0) continue;
    if (s.qc->vis && !s.qc->vis(s.qc->vis_ctx, me, ed.origin, true)) continue;
    out.push_back(grid_entity(st, c.coords));
  }
  if (static_cast<int>(out.size()) > limit) {
    std::stable_sort(out.begin(), out.end(), [&](const EntityState& a, const EntityState& b) {
      const sh::Vec3f da = a.origin - me, db = b.origin - me;
      return sh::dot(da, da) < sh::dot(db, db);
    });
    out.resize(static_cast<size_t>(limit));
    std::sort(out.begin(), out.end(),
              [](const EntityState& a, const EntityState& b) { return a.number < b.number; });
  }
}

void send_static_updates(ClassicServer& s) {
  const int n = static_cast<int>(s.qc->statics.size());
  for (; s.statics_known < n; s.statics_known++) {
    const sv::StaticEntity& st = s.qc->statics[static_cast<size_t>(s.statics_known)];
    for (int j = 0; j < sh::kMaxClients; j++) {
      ClassicClient& c = s.clients[j];
      if (c.state != ConnState::spawned) continue;
      SvcMsg m;
      m.id = svc::spawnstatic;
      m.ints = {st.modelindex, static_cast<int>(st.frame), static_cast<int>(st.colormap),
                static_cast<int>(st.skin)};
      m.floats = {st.origin.x, st.angles.x, st.origin.y, st.angles.y, st.origin.z, st.angles.z};
      reliable_svc(s, j, m);
    }
  }
}

}  // namespace

void route_events(ClassicServer& s) {
  sv::MessageSink& out = s.qc->out;
  for (const sv::NetEvent& ev : out.events) route_event(s, ev);
  s.counts.events += static_cast<int64_t>(out.events.size());
  send_static_updates(s);
  for (int j = 0; j < sh::kMaxClients; j++) {
    ClassicClient& c = s.clients[j];
    if (c.state != ConnState::spawned) continue;
    mod_bytes(s, j, out.all.data(), out.all.size(), true);
    mod_bytes(s, j, out.one[j].data(), out.one[j].size(), true);
    mod_bytes(s, j, out.unreliable.data(), out.unreliable.size(), false);
  }
  out.all.clear();
  for (std::vector<uint8_t>& one : out.one) one.clear();
  sv::clear_frame(out);
  for (int j = 0; j < sh::kMaxClients; j++)
    if (s.clients[j].state == ConnState::spawned) update_stats(s, j);
}

void send_frames(ClassicServer& s) {
  uint8_t payload[kMaxMsgLen];
  uint8_t packet[kMaxPacket];
  std::vector<EntityState> ents;
  for (int j = 0; j < sh::kMaxClients; j++) {
    ClassicClient& c = s.clients[j];
    if (c.state == ConnState::free || !c.heard) continue;
    if (!channel_can_packet(c.ch, s.now)) {
      c.chokecount++;
      c.ch.choked++;
      s.counts.choked++;
      continue;
    }
    const size_t room =
        kMaxPacket - kServerHeader - c.ch.reliable.size -
        (c.ch.reliable.size == 0 && c.ch.pending_count > 0 ? c.ch.pending[0].size : 0);
    gn::Writer w = gn::make_writer(payload, std::min(room, sizeof payload));
    const uint32_t seq = c.ch.outgoing_sequence;
    ServerFrame& f = c.frames[seq & 63];
    f.sequence = seq;
    f.sent = s.now;
    f.server_time = s.qc->sv.time;
    f.entities = false;
    if (c.state == ConnState::spawned) {
      if (c.chokecount) {
        write_byte(w, svc::chokecount);
        write_byte(w, c.chokecount > 255 ? 255 : c.chokecount);
        c.chokecount = 0;
      }
      for (int k = 0; k < sh::kMaxClients; k++) {
        if (!s.clients[k].qc_active || s.clients[k].spectator) continue;
        if (!s.qc->sv.clients[static_cast<size_t>(k)].active) continue;
        const sh::Vec3f me = s.qc->sv.edicts[static_cast<size_t>(j) + 1].origin;
        const sh::Vec3f it = s.qc->sv.edicts[static_cast<size_t>(k) + 1].origin;
        if (k != j && s.qc->vis && !s.qc->vis(s.qc->vis_ctx, me, it, true)) continue;
        write_byte(w, svc::playerinfo);
        write_playerinfo(w, player_state(s, k, j), c.ext);
      }
      visible_entities(s, j, ents);
      const std::vector<EntityState>* from = nullptr;
      if (c.delta_byte >= 0) {
        const uint32_t full = seq - ((seq - static_cast<uint32_t>(c.delta_byte)) & 0xffu);
        const ServerFrame& df = c.frames[full & 63];
        if (seq - full < 64 && full != seq && df.sequence == full && df.entities) {
          from = &df.ents;
          write_byte(w, svc::deltapacketentities);
          write_byte(w, static_cast<int>(full & 0xff));
        }
      }
      if (from == nullptr) write_byte(w, svc::packetentities);
      write_packet_entities(w, from, ents, s.baselines, c.ext);
      f.ents = ents;
      f.entities = !w.overflow;
      if (!w.overflow && w.size + c.datagram.size() <= w.capacity)
        gn::write_bytes(w, c.datagram.data(), c.datagram.size());
    }
    // Downloads run during the signon too (the skins step comes before `begin`).
    if (!w.overflow) send_download_chunks(s, j, w);
    c.datagram.clear();
    if (w.overflow) {
      w.overflow = false;
      w.size = 0;  // nothing but the reliable part fits this time
      f.entities = false;
    }
    const size_t n = channel_transmit(c.ch, payload, w.size, s.now, packet);
    for (int k = 0; k < c.ch.copies; k++)
      gn::transport_send(*s.transport, c.address, packet, n, static_cast<int64_t>(s.now * 1e6));
    s.counts.datagrams_out += c.ch.copies;
    s.counts.bytes_out += static_cast<int64_t>(n) * c.ch.copies;
    c.heard = false;
  }
}

}  // namespace shooters_edge::net

// Entity and player deltas (classic_delta.hpp).
//
// ge:summary The entity records, packet-entity lists and player states, written and read
// ge:keywords entity_bits write_entity_delta read_entity_record write_packet_entities
// ge:keywords read_packet_entities write_playerinfo read_playerinfo
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_messages_test.cpp
#include "shooters_edge/net/classic_delta.hpp"

namespace shooters_edge::net {

namespace {

const EntityState kNull{};

float angle_grid(float a, Coords c) {
  return c == Coords::all ? angle16_from_wire(angle16_to_wire(a))
                          : angle_from_wire(angle_to_wire(a));
}
float origin_grid(float f, Coords c) { return c == Coords::shorts ? coord_grid(f) : f; }

uint32_t number_bits(int number) {
  uint32_t b = 0;
  if (number >= 1024) {
    b |= kUEntityDbl2;
    number -= 1024;
  }
  if (number >= 512) b |= kUEntityDbl;
  return b;
}

// The word, the more byte and the even-more byte for `bits` and the number.
void write_flags(gn::Writer& w, int number, uint32_t bits) {
  if (bits & 0xffff0000u) bits |= kUEvenMore;
  if (bits & 0xffu) bits |= kUMoreBits;
  gn::write_u16(w, static_cast<uint16_t>((number & 511) | (bits & 0xfe00u)));
  if (bits & kUMoreBits) gn::write_u8(w, static_cast<uint8_t>(bits & 0xff));
  if (bits & kUEvenMore) gn::write_u8(w, static_cast<uint8_t>((bits >> 16) & 0xff));
  if (bits & kUYetMore) gn::write_u8(w, static_cast<uint8_t>((bits >> 24) & 0xff));
}

}  // namespace

EntityState grid_entity(const EntityState& s, Coords coords) {
  EntityState g = s;
  for (int i = 0; i < 3; i++) {
    g.origin[i] = origin_grid(s.origin[i], coords);
    g.angles[i] = angle_grid(s.angles[i], coords);
  }
  return g;
}

uint32_t entity_bits(const EntityState& from, const EntityState& to, const Ext& ext) {
  uint32_t b = 0;
  if (to.origin.x != from.origin.x) b |= kUOrigin1;
  if (to.origin.y != from.origin.y) b |= kUOrigin2;
  if (to.origin.z != from.origin.z) b |= kUOrigin3;
  if (to.angles.x != from.angles.x) b |= kUAngle1;
  if (to.angles.y != from.angles.y) b |= kUAngle2;
  if (to.angles.z != from.angles.z) b |= kUAngle3;
  if (to.frame != from.frame) b |= kUFrame;
  if (to.colormap != from.colormap) b |= kUColormap;
  if (to.skin != from.skin) b |= kUSkin;
  if (to.effects != from.effects) b |= kUEffects;
  if (to.modelindex != from.modelindex)
    b |= to.modelindex > 255 && ext.has(ExtFamily::x, kXModelDbl) ? kUModelDbl : kUModel;
  if (to.alpha != from.alpha && ext.has(ExtFamily::x, kXTrans)) b |= kUTrans;
  return b;
}

bool write_entity_delta(gn::Writer& w, const EntityState& from, const EntityState& to, bool force,
                        const Ext& ext) {
  uint32_t bits = entity_bits(from, to, ext);
  if (bits == 0 && !force) return false;
  const Coords coords = coords_of(ext);
  bits |= number_bits(to.number);
  write_flags(w, to.number, bits);
  if (bits & kUModel) write_byte(w, to.modelindex);
  if (bits & kUModelDbl) write_short(w, to.modelindex);
  if (bits & kUFrame) write_byte(w, to.frame);
  if (bits & kUColormap) write_byte(w, to.colormap);
  if (bits & kUSkin) write_byte(w, to.skin);
  if (bits & kUEffects) write_byte(w, to.effects);
  if (bits & kUOrigin1) write_origin(w, to.origin.x, coords);
  if (bits & kUAngle1) write_angle(w, to.angles.x, coords);
  if (bits & kUOrigin2) write_origin(w, to.origin.y, coords);
  if (bits & kUAngle2) write_angle(w, to.angles.y, coords);
  if (bits & kUOrigin3) write_origin(w, to.origin.z, coords);
  if (bits & kUAngle3) write_angle(w, to.angles.z, coords);
  if (bits & kUTrans) write_byte(w, to.alpha);
  return true;
}

void write_entity_remove(gn::Writer& w, int number) {
  write_flags(w, number, kURemove | number_bits(number));
}

EntityRecord read_entity_record(gn::Reader& r, uint16_t word, const EntityState& from,
                                const Ext& ext) {
  EntityRecord rec;
  uint32_t bits = word & 0xfe00u;
  int number = word & 511;
  if (bits & kUMoreBits) bits |= static_cast<uint32_t>(read_byte(r));
  if (bits & kUEvenMore) bits |= static_cast<uint32_t>(read_byte(r)) << 16;
  if (bits & kUYetMore) bits |= static_cast<uint32_t>(read_byte(r)) << 24;
  if (bits & kUEntityDbl) number += 512;
  if (bits & kUEntityDbl2) number += 1024;
  rec.number = number;
  rec.bits = bits;
  EntityState s = from;
  s.number = number;
  const Coords coords = coords_of(ext);
  if (bits & kUModel) s.modelindex = read_byte(r) + ((bits & kUModelDbl) ? 256 : 0);
  if ((bits & kUModelDbl) && !(bits & kUModel)) s.modelindex = read_short(r) & 0xffff;
  if (bits & kUFrame) s.frame = read_byte(r);
  if (bits & kUColormap) s.colormap = read_byte(r);
  if (bits & kUSkin) s.skin = read_byte(r);
  if (bits & kUEffects) s.effects = read_byte(r);
  if (bits & kUOrigin1) s.origin.x = read_origin(r, coords);
  if (bits & kUAngle1) s.angles.x = read_angle(r, coords);
  if (bits & kUOrigin2) s.origin.y = read_origin(r, coords);
  if (bits & kUAngle2) s.angles.y = read_angle(r, coords);
  if (bits & kUOrigin3) s.origin.z = read_origin(r, coords);
  if (bits & kUAngle3) s.angles.z = read_angle(r, coords);
  if (bits & kUTrans) s.alpha = read_byte(r);
  rec.state = s;
  return rec;
}

void write_packet_entities(gn::Writer& w, const std::vector<EntityState>* from,
                           const std::vector<EntityState>& to,
                           const std::vector<EntityState>& baselines, const Ext& ext) {
  size_t i = 0, j = 0;
  const size_t nf = from ? from->size() : 0;
  while (i < nf || j < to.size()) {
    const int fnum = i < nf ? (*from)[i].number : 1 << 30;
    const int tnum = j < to.size() ? to[j].number : 1 << 30;
    if (fnum == tnum) {
      write_entity_delta(w, (*from)[i], to[j], false, ext);
      i++;
      j++;
    } else if (tnum < fnum) {
      const EntityState& base = static_cast<size_t>(tnum) < baselines.size()
                                    ? baselines[static_cast<size_t>(tnum)]
                                    : kNull;
      write_entity_delta(w, base, to[j], true, ext);
      j++;
    } else {
      write_entity_remove(w, fnum);
      i++;
    }
  }
  gn::write_u16(w, 0);
}

bool read_packet_entities(gn::Reader& r, const std::vector<EntityState>* from,
                          const std::vector<EntityState>& baselines, const Ext& ext,
                          std::vector<EntityState>& out, std::vector<EntityRecord>* records) {
  out.clear();
  size_t i = 0;
  const size_t nf = from ? from->size() : 0;
  int last = -1;
  while (true) {
    const uint16_t word = gn::read_u16(r);
    if (r.bad) return false;
    if (word == 0) break;
    const EntityRecord peek = [&] {
      // The number alone, to walk the from-list before reading the record against its entry.
      int n = word & 511;
      if (word & kUMoreBits) {
        const uint8_t more = r.at < r.size ? r.data[r.at] : 0;
        if ((more & kUEvenMore) && r.at + 1 < r.size) {
          const uint32_t even = static_cast<uint32_t>(r.data[r.at + 1]) << 16;
          if (even & kUEntityDbl) n += 512;
          if (even & kUEntityDbl2) n += 1024;
        }
      }
      EntityRecord p;
      p.number = n;
      return p;
    }();
    const int num = peek.number;
    if (num <= last) return false;  // out of order: damaged
    last = num;
    while (i < nf && (*from)[i].number < num) out.push_back((*from)[i++]);
    const bool have = i < nf && (*from)[i].number == num;
    const EntityState& base = have ? (*from)[i]
                              : static_cast<size_t>(num) < baselines.size()
                                  ? baselines[static_cast<size_t>(num)]
                                  : kNull;
    EntityRecord rec = read_entity_record(r, word, base, ext);
    if (r.bad) return false;
    if (have) i++;
    if (records) records->push_back(rec);
    if (!(rec.bits & kURemove)) out.push_back(rec.state);
  }
  while (i < nf) out.push_back((*from)[i++]);
  return !r.bad;
}

uint32_t playerinfo_flags(const PlayerState& p, const Ext& ext) {
  uint32_t f = 0;
  if (p.has_msec) f |= kPfMsec;
  if (p.has_command) f |= kPfCommand;
  if (static_cast<int>(p.velocity.x) != 0) f |= kPfVelocity1;
  if (static_cast<int>(p.velocity.y) != 0) f |= kPfVelocity2;
  if (static_cast<int>(p.velocity.z) != 0) f |= kPfVelocity3;
  if (p.modelindex != 0) f |= kPfModel;
  if (p.skin != 0 || p.modelindex > 255) f |= kPfSkin;
  if (p.effects != 0) f |= kPfEffects;
  if (p.has_weaponframe) f |= kPfWeaponFrame;
  if (p.dead) f |= kPfDead;
  if (p.gib) f |= kPfGib;
  f |= static_cast<uint32_t>(p.pm) << kPfPmTypeShift;
  const bool extended = ext.has(ExtFamily::x, kXTrans);
  if (extended) {
    if (p.onground && ext.has(ExtFamily::z, kZOnGround)) f |= kPfOnGroundX;
    if (p.solid && ext.has(ExtFamily::z, kZSolid)) f |= kPfSolidX;
    if (p.alpha != 0) f |= kPfTrans;
    if (f & 0xffff0000u) f |= kPfExtra;
  } else {
    if (p.onground && ext.has(ExtFamily::z, kZOnGround)) f |= kPfOnGround;
    if (p.solid && ext.has(ExtFamily::z, kZSolid)) f |= kPfSolid;
  }
  return f;
}

void write_playerinfo(gn::Writer& w, const PlayerState& p, const Ext& ext) {
  const uint32_t f = playerinfo_flags(p, ext);
  const Coords coords = coords_of(ext);
  write_byte(w, p.slot);
  write_short(w, static_cast<int>(f & 0xffff));
  if (ext.has(ExtFamily::x, kXTrans) && (f & kPfExtra)) write_byte(w, static_cast<int>(f >> 16));
  for (int i = 0; i < 3; i++) write_origin(w, p.origin[i], coords);
  write_byte(w, p.frame);
  if (f & kPfMsec) write_byte(w, p.msec);
  if (f & kPfCommand) write_delta_usercmd(w, sh::MoveCmd{}, p.command);
  if (f & kPfVelocity1) write_short(w, static_cast<int>(p.velocity.x));
  if (f & kPfVelocity2) write_short(w, static_cast<int>(p.velocity.y));
  if (f & kPfVelocity3) write_short(w, static_cast<int>(p.velocity.z));
  if (f & kPfModel) write_byte(w, p.modelindex & 255);
  if (f & kPfSkin) write_byte(w, (p.skin & 127) | (p.modelindex > 255 ? 128 : 0));
  if (f & kPfEffects) write_byte(w, p.effects);
  if (f & kPfWeaponFrame) write_byte(w, p.weaponframe);
  if (f & kPfTrans) write_byte(w, p.alpha);
}

PlayerState read_playerinfo(gn::Reader& r, const Ext& ext, uint32_t* flags_out) {
  PlayerState p;
  const Coords coords = coords_of(ext);
  p.slot = read_byte(r);
  uint32_t f = static_cast<uint16_t>(read_short(r));
  const bool extended = ext.has(ExtFamily::x, kXTrans);
  if (extended && (f & kPfExtra)) f |= static_cast<uint32_t>(read_byte(r)) << 16;
  if (flags_out) *flags_out = f;
  for (int i = 0; i < 3; i++) p.origin[i] = read_origin(r, coords);
  p.frame = read_byte(r);
  p.has_msec = (f & kPfMsec) != 0;
  if (p.has_msec) p.msec = read_byte(r);
  p.has_command = (f & kPfCommand) != 0;
  if (p.has_command) p.command = read_delta_usercmd(r, sh::MoveCmd{});
  if (f & kPfVelocity1) p.velocity.x = static_cast<float>(read_short(r));
  if (f & kPfVelocity2) p.velocity.y = static_cast<float>(read_short(r));
  if (f & kPfVelocity3) p.velocity.z = static_cast<float>(read_short(r));
  if (f & kPfModel) p.modelindex = read_byte(r);
  if (f & kPfSkin) {
    const int s = read_byte(r);
    p.skin = s & 127;
    if (s & 128) p.modelindex += 256;
  }
  if (f & kPfEffects) p.effects = read_byte(r);
  p.has_weaponframe = (f & kPfWeaponFrame) != 0;
  if (p.has_weaponframe) p.weaponframe = read_byte(r);
  p.dead = (f & kPfDead) != 0;
  p.gib = (f & kPfGib) != 0;
  p.pm = static_cast<PmType>((f >> kPfPmTypeShift) & 7);
  if (extended) {
    p.onground = (f & kPfOnGroundX) != 0;
    p.solid = (f & kPfSolidX) != 0;
    if (f & kPfTrans) p.alpha = read_byte(r);
  } else {
    p.onground = (f & kPfOnGround) != 0;
    p.solid = (f & kPfSolid) != 0;
  }
  return p;
}

}  // namespace shooters_edge::net

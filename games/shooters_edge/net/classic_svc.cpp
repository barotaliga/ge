// The classic messages (classic_svc.hpp): the table, the layouts' codec and the special bodies.
//
// ge:summary The svc table and clc ids, every body's writer and reader, and one-line descriptions
// ge:keywords ksvcdefs write_svc read_svc write_clc read_clc describe_svc write_move
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_messages_test.cpp
#include "shooters_edge/net/classic_svc.hpp"

#include "core/num_text.hpp"

namespace shooters_edge::net {

namespace {

const SvcDef kDefs[] = {
    {svc::nop, false, "nop", ""},
    {svc::disconnect, false, "disconnect", ""},
    {svc::updatestat, true, "updatestat", "bb"},
    {svc::sound, false, "sound", "*"},
    {svc::print, true, "print", "bs"},
    {svc::stufftext, true, "stufftext", "s"},
    {svc::setangle, true, "setangle", "*"},
    {svc::serverdata, true, "serverdata", "*"},
    {svc::lightstyle, true, "lightstyle", "bs"},
    {svc::updatefrags, true, "updatefrags", "bh"},
    {svc::stopsound, false, "stopsound", "h"},
    {svc::damage, false, "damage", "bbCCC"},
    {svc::spawnstatic, true, "spawnstatic", "bbbbOAOAOA"},
    {svc::spawnstatic2, true, "spawnstatic2", "*"},
    {svc::spawnbaseline, true, "spawnbaseline", "hbbbbOAOAOA"},
    {svc::temp_entity, false, "temp_entity", "*"},
    {svc::setpause, true, "setpause", "b"},
    {svc::centerprint, true, "centerprint", "s"},
    {svc::killedmonster, true, "killedmonster", ""},
    {svc::foundsecret, true, "foundsecret", ""},
    {svc::spawnstaticsound, true, "spawnstaticsound", "CCCbbb"},
    {svc::intermission, true, "intermission", "CCCAAA"},
    {svc::finale, true, "finale", "s"},
    {svc::cdtrack, true, "cdtrack", "b"},
    {svc::sellscreen, true, "sellscreen", ""},
    {svc::smallkick, false, "smallkick", ""},
    {svc::bigkick, false, "bigkick", ""},
    {svc::updateping, true, "updateping", "bh"},
    {svc::updateentertime, true, "updateentertime", "bf"},
    {svc::updatestatlong, true, "updatestatlong", "bl"},
    {svc::muzzleflash, false, "muzzleflash", "h"},
    {svc::updateuserinfo, true, "updateuserinfo", "bls"},
    {svc::download, false, "download", "*"},
    {svc::playerinfo, false, "playerinfo", "*"},
    {svc::nails, false, "nails", "*"},
    {svc::chokecount, false, "chokecount", "b"},
    {svc::modellist, true, "modellist", "*"},
    {svc::soundlist, true, "soundlist", "*"},
    {svc::packetentities, false, "packetentities", "*"},
    {svc::deltapacketentities, false, "deltapacketentities", "*"},
    {svc::maxspeed, true, "maxspeed", "f"},
    {svc::entgravity, true, "entgravity", "f"},
    {svc::setinfo, true, "setinfo", "bss"},
    {svc::serverinfo, true, "serverinfo", "ss"},
    {svc::updatepl, true, "updatepl", "bb"},
    {svc::soundextended, false, "soundextended", "*"},
    {svc::soundlistshort, true, "soundlistshort", "*"},
    {svc::modellistshort, true, "modellistshort", "*"},
    {svc::spawnbaseline2, true, "spawnbaseline2", "*"},
    {svc::updatestatstring, true, "updatestatstring", "bs"},
    {svc::updatestatfloat, true, "updatestatfloat", "bf"},
    {svc::spawnstaticsound2, true, "spawnstaticsound2", "CCCbhbb"},
};
constexpr int kDefCount = static_cast<int>(sizeof kDefs / sizeof kDefs[0]);

const EntityState kNull{};

// Writes `layout` from the message's fields starting at `ii`, `fi`, `si`.
void write_layout(gn::Writer& w, const char* layout, const SvcMsg& m, size_t& ii, size_t& fi,
                  size_t& si, Coords coords) {
  for (const char* k = layout; *k; k++) {
    const int32_t iv = ii < m.ints.size() ? m.ints[ii] : 0;
    const float fv = fi < m.floats.size() ? m.floats[fi] : 0;
    switch (*k) {
      case 'b':
        write_byte(w, iv);
        ii++;
        break;
      case 'c':
        write_char(w, iv);
        ii++;
        break;
      case 'h':
        write_short(w, iv);
        ii++;
        break;
      case 'l':
        write_long(w, iv);
        ii++;
        break;
      case 'f':
        write_float(w, fv);
        fi++;
        break;
      case 'C':
        write_coord(w, fv, coords);
        fi++;
        break;
      case 'O':
        write_origin(w, fv, coords);
        fi++;
        break;
      case 'A':
        write_angle(w, fv, coords);
        fi++;
        break;
      case 's':
        write_string(w, si < m.strings.size() ? m.strings[si] : std::string());
        si++;
        break;
      default:
        break;
    }
  }
}

void read_layout(gn::Reader& r, const char* layout, SvcMsg& m, Coords coords) {
  for (const char* k = layout; *k; k++) {
    switch (*k) {
      case 'b':
        m.ints.push_back(read_byte(r));
        break;
      case 'c':
        m.ints.push_back(read_char(r));
        break;
      case 'h':
        m.ints.push_back(read_short(r));
        break;
      case 'l':
        m.ints.push_back(read_long(r));
        break;
      case 'f':
        m.floats.push_back(read_float(r));
        break;
      case 'C':
        m.floats.push_back(read_coord(r, coords));
        break;
      case 'O':
        m.floats.push_back(read_origin(r, coords));
        break;
      case 'A':
        m.floats.push_back(read_angle(r, coords));
        break;
      case 's':
        m.strings.push_back(read_string(r));
        break;
      default:
        break;
    }
  }
}

int32_t at(const std::vector<int32_t>& v, size_t i) { return i < v.size() ? v[i] : 0; }
float atf(const std::vector<float>& v, size_t i) { return i < v.size() ? v[i] : 0; }

bool is_list(uint8_t id) {
  return id == svc::modellist || id == svc::soundlist || id == svc::modellistshort ||
         id == svc::soundlistshort;
}

}  // namespace

const SvcDef* svc_def(uint8_t id) {
  for (const SvcDef& d : kDefs)
    if (d.id == id) return &d;
  return nullptr;
}
const SvcDef* svc_defs(int& count) {
  count = kDefCount;
  return kDefs;
}

const char* temp_entity_layout(uint8_t kind) {
  switch (kind) {
    case te::spike:
    case te::superspike:
    case te::explosion:
    case te::tarexplosion:
    case te::wizspike:
    case te::knightspike:
    case te::lavasplash:
    case te::teleport:
    case te::lightningblood:
      return "CCC";
    case te::gunshot:
    case te::blood:
      return "bCCC";
    case te::lightning1:
    case te::lightning2:
    case te::lightning3:
      return "hCCCCCC";
    default:
      return nullptr;
  }
}

bool write_svc(gn::Writer& w, const SvcMsg& m, const WireCtx& ctx) {
  const SvcDef* d = svc_def(m.id);
  if (d == nullptr) return false;
  const Coords coords = coords_of(ctx.ext);
  write_byte(w, m.id);
  size_t ii = 0, fi = 0, si = 0;
  if (d->layout[0] != '*') {
    write_layout(w, d->layout, m, ii, fi, si, coords);
    return true;
  }
  switch (m.id) {
    case svc::sound: {
      const int ent = at(m.ints, 0), ch = at(m.ints, 1), vol = at(m.ints, 2);
      const int att = at(m.ints, 3);
      int packed = (ent << 3) | (ch & 7);
      if (vol != 255) packed |= 0x8000;
      if (att != 64) packed |= 0x4000;
      write_short(w, packed);
      if (vol != 255) write_byte(w, vol);
      if (att != 64) write_byte(w, att);
      write_byte(w, at(m.ints, 4));
      for (int i = 0; i < 3; i++) write_coord(w, atf(m.floats, static_cast<size_t>(i)), coords);
      return true;
    }
    case svc::soundextended: {
      const int ent = at(m.ints, 0), ch = at(m.ints, 1), vol = at(m.ints, 2);
      const int att = at(m.ints, 3), snd = at(m.ints, 4);
      int flags = 0;
      if (vol != 255) flags |= 1;
      if (att != 64) flags |= 2;
      if (ent >= 8192) flags |= 8;
      if (snd > 255) flags |= 0x10;
      write_byte(w, flags);
      if (flags & 1) write_byte(w, vol);
      if (flags & 2) write_byte(w, att);
      if (flags & 8) {
        write_short(w, ent);
        write_byte(w, ch);
      } else {
        write_short(w, (ent << 3) | (ch & 7));
      }
      if (flags & 0x10)
        write_short(w, snd);
      else
        write_byte(w, snd);
      for (int i = 0; i < 3; i++) write_coord(w, atf(m.floats, static_cast<size_t>(i)), coords);
      return true;
    }
    case svc::setangle:
      if (ctx.ext.has(ExtFamily::mvd, kMvdTeleport)) write_byte(w, at(m.ints, 0));
      for (int i = 0; i < 3; i++) write_angle(w, atf(m.floats, static_cast<size_t>(i)), coords);
      return true;
    case svc::serverdata:
      for (const auto& [magic, bits] : m.ext) {
        gn::write_u32(w, magic);
        gn::write_u32(w, bits);
      }
      write_long(w, at(m.ints, 0));
      write_long(w, at(m.ints, 1));
      write_string(w, m.strings.empty() ? std::string() : m.strings[0]);
      write_byte(w, at(m.ints, 2));
      write_string(w, m.strings.size() < 2 ? std::string() : m.strings[1]);
      for (int i = 0; i < 10; i++) write_float(w, atf(m.floats, static_cast<size_t>(i)));
      return true;
    case svc::spawnstatic2:
      write_entity_delta(w, kNull, m.ents.empty() ? kNull : m.ents[0].state, true, ctx.ext);
      return true;
    case svc::spawnbaseline2:
      write_short(w, at(m.ints, 0));
      write_entity_delta(w, kNull, m.ents.empty() ? kNull : m.ents[0].state, true, ctx.ext);
      return true;
    case svc::temp_entity: {
      const auto kind = static_cast<uint8_t>(at(m.ints, 0));
      const char* layout = temp_entity_layout(kind);
      if (layout == nullptr) return false;
      write_byte(w, kind);
      ii = 1;
      write_layout(w, layout, m, ii, fi, si, coords);
      return true;
    }
    case svc::download:
      if (ctx.chunked) {
        write_long(w, at(m.ints, 0));
        if (at(m.ints, 0) == -1) {
          write_long(w, at(m.ints, 1));
          write_string(w, m.strings.empty() ? std::string() : m.strings[0]);
        } else {
          uint8_t block[kChunkBytes] = {};
          for (size_t i = 0; i < m.data.size() && i < sizeof block; i++) block[i] = m.data[i];
          gn::write_bytes(w, block, sizeof block);
        }
      } else {
        write_short(w, at(m.ints, 0));
        write_byte(w, at(m.ints, 1));
        if (at(m.ints, 0) > 0) gn::write_bytes(w, m.data.data(), m.data.size());
      }
      return true;
    case svc::playerinfo:
      write_playerinfo(w, m.player, ctx.ext);
      return true;
    case svc::nails:
      write_byte(w, at(m.ints, 0));
      gn::write_bytes(w, m.data.data(), m.data.size());
      return true;
    case svc::packetentities:
    case svc::deltapacketentities:
      if (m.id == svc::deltapacketentities) write_byte(w, at(m.ints, 0));
      for (const EntityRecord& e : m.ents) {
        if (e.bits & kURemove)
          write_entity_remove(w, e.number);
        else
          write_entity_delta(w, kNull, e.state, true, ctx.ext);
      }
      gn::write_u16(w, 0);
      return true;
    default:
      if (is_list(m.id)) {
        if (m.id == svc::modellistshort || m.id == svc::soundlistshort)
          write_short(w, at(m.ints, 0));
        else
          write_byte(w, at(m.ints, 0));
        for (const std::string& s : m.strings) write_string(w, s);
        write_string(w, "");
        write_byte(w, at(m.ints, 1));
        return true;
      }
      return false;
  }
}

bool read_svc(gn::Reader& r, SvcMsg& m, const WireCtx& ctx, std::string& error) {
  m = SvcMsg{};
  m.id = static_cast<uint8_t>(read_byte(r));
  const SvcDef* d = svc_def(m.id);
  if (r.bad) {
    error = "read_svc: the payload ended before a message id";
    return false;
  }
  if (d == nullptr) {
    error = "read_svc: unknown message id " + std::to_string(m.id) + " at byte " +
            std::to_string(r.at - 1) +
            "; next: the stream cannot be resynced; check the agreed extensions (classic_ext.hpp)";
    return false;
  }
  const Coords coords = coords_of(ctx.ext);
  if (d->layout[0] != '*') {
    read_layout(r, d->layout, m, coords);
  } else {
    switch (m.id) {
      case svc::sound: {
        const int packed = read_short(r) & 0xffff;
        const int vol = (packed & 0x8000) ? read_byte(r) : 255;
        const int att = (packed & 0x4000) ? read_byte(r) : 64;
        const int snd = read_byte(r);
        m.ints = {(packed >> 3) & 1023, packed & 7, vol, att, snd};
        for (int i = 0; i < 3; i++) m.floats.push_back(read_coord(r, coords));
        break;
      }
      case svc::soundextended: {
        const int flags = read_byte(r);
        const int vol = (flags & 1) ? read_byte(r) : 255;
        const int att = (flags & 2) ? read_byte(r) : 64;
        int ent, ch;
        if (flags & 8) {
          ent = read_short(r) & 0xffff;
          ch = read_byte(r);
        } else {
          const int packed = read_short(r) & 0xffff;
          ent = packed >> 3;
          ch = packed & 7;
        }
        const int snd = (flags & 0x10) ? (read_short(r) & 0xffff) : read_byte(r);
        m.ints = {ent, ch, vol, att, snd};
        for (int i = 0; i < 3; i++) m.floats.push_back(read_coord(r, coords));
        break;
      }
      case svc::setangle:
        m.ints.push_back(ctx.ext.has(ExtFamily::mvd, kMvdTeleport) ? read_byte(r) : -1);
        for (int i = 0; i < 3; i++) m.floats.push_back(read_angle(r, coords));
        break;
      case svc::serverdata: {
        int32_t v = read_long(r);
        ExtFamily f{};
        while (!r.bad && v != kProtocolVersion && family_of_magic(static_cast<uint32_t>(v), f)) {
          m.ext.emplace_back(static_cast<uint32_t>(v), gn::read_u32(r));
          v = read_long(r);
        }
        m.ints.push_back(v);
        m.ints.push_back(read_long(r));
        m.strings.push_back(read_string(r));
        m.ints.push_back(read_byte(r));
        m.strings.push_back(read_string(r));
        for (int i = 0; i < 10; i++) m.floats.push_back(read_float(r));
        break;
      }
      case svc::spawnbaseline2:
        m.ints.push_back(read_short(r));
        [[fallthrough]];
      case svc::spawnstatic2: {
        const uint16_t word = gn::read_u16(r);
        m.ents.push_back(read_entity_record(r, word, kNull, ctx.ext));
        break;
      }
      case svc::temp_entity: {
        const auto kind = static_cast<uint8_t>(read_byte(r));
        const char* layout = temp_entity_layout(kind);
        if (layout == nullptr) {
          error = "read_svc: unknown temporary entity kind " + std::to_string(kind) +
                  "; next: the te:: table in classic_svc.hpp";
          return false;
        }
        m.ints.push_back(kind);
        read_layout(r, layout, m, coords);
        break;
      }
      case svc::download:
        if (ctx.chunked) {
          const int32_t first = read_long(r);
          m.ints.push_back(first);
          if (first == -1) {
            m.ints.push_back(read_long(r));
            m.strings.push_back(read_string(r));
          } else {
            const uint8_t* p = gn::read_view(r, kChunkBytes);
            if (p) m.data.assign(p, p + kChunkBytes);
          }
        } else {
          const int size = read_short(r);
          m.ints.push_back(size);
          m.ints.push_back(read_byte(r));
          if (size > 0) {
            const uint8_t* p = gn::read_view(r, static_cast<size_t>(size));
            if (p) m.data.assign(p, p + size);
          }
        }
        break;
      case svc::playerinfo:
        m.player = read_playerinfo(r, ctx.ext, &m.player_flags);
        break;
      case svc::nails: {
        const int n = read_byte(r);
        m.ints.push_back(n);
        const size_t bytes = static_cast<size_t>(n) * 6;
        const uint8_t* p = gn::read_view(r, bytes);
        if (p) m.data.assign(p, p + bytes);
        break;
      }
      case svc::packetentities:
      case svc::deltapacketentities: {
        if (m.id == svc::deltapacketentities) m.ints.push_back(read_byte(r));
        std::vector<EntityState> out;
        const std::vector<EntityState> none;
        if (!read_packet_entities(r, nullptr, none, ctx.ext, out, &m.ents)) {
          error = "read_svc: a damaged entity list; next: compare the agreed extensions";
          return false;
        }
        break;
      }
      default:
        if (is_list(m.id)) {
          const bool shortc = m.id == svc::modellistshort || m.id == svc::soundlistshort;
          m.ints.push_back(shortc ? (read_short(r) & 0xffff) : read_byte(r));
          while (!r.bad) {
            std::string s = read_string(r);
            if (s.empty()) break;
            m.strings.push_back(std::move(s));
          }
          m.ints.push_back(read_byte(r));
        }
        break;
    }
  }
  if (r.bad) {
    error = std::string("read_svc: ") + d->name + "'s body runs past the payload's end";
    return false;
  }
  return true;
}

std::string describe_svc(const SvcMsg& m) {
  const SvcDef* d = svc_def(m.id);
  std::string s = d ? d->name : "svc " + std::to_string(m.id);
  for (int32_t v : m.ints) s += " " + std::to_string(v);
  for (float f : m.floats) s += " " + ge::core::num::to_fixed(f, 3);
  for (const std::string& t : m.strings) s += " \"" + t + "\"";
  for (const auto& [magic, bits] : m.ext)
    s += " ext:" + to_hex(reinterpret_cast<const uint8_t*>(&magic), 4) + "=" + std::to_string(bits);
  if (!m.data.empty()) s += " +" + std::to_string(m.data.size()) + " bytes";
  if (!m.ents.empty()) s += " ents:" + std::to_string(m.ents.size());
  if (m.id == svc::playerinfo)
    s += " slot " + std::to_string(m.player.slot) + " flags " + std::to_string(m.player_flags);
  return s;
}

const char* clc_name(uint8_t id) {
  switch (id) {
    case clc::nop:
      return "nop";
    case clc::move:
      return "move";
    case clc::stringcmd:
      return "stringcmd";
    case clc::delta:
      return "delta";
    case clc::tmove:
      return "tmove";
    case clc::upload:
      return "upload";
    default:
      return nullptr;
  }
}

void write_move(gn::Writer& w, const sh::MoveCmd cmds[3], uint8_t loss, uint32_t sequence) {
  write_byte(w, clc::move);
  const size_t checksum_at = w.size;
  write_byte(w, 0);
  const size_t body_at = w.size;
  write_byte(w, loss);
  write_delta_usercmd(w, sh::MoveCmd{}, cmds[0]);
  write_delta_usercmd(w, cmds[0], cmds[1]);
  write_delta_usercmd(w, cmds[1], cmds[2]);
  if (!w.overflow)
    w.data[checksum_at] = move_checksum(w.data + body_at, w.size - body_at, sequence);
}

bool write_clc(gn::Writer& w, const ClcMsg& m) {
  switch (m.id) {
    case clc::nop:
      write_byte(w, m.id);
      return true;
    case clc::move: {
      write_byte(w, m.id);
      write_byte(w, m.checksum);
      write_byte(w, m.loss);
      write_delta_usercmd(w, sh::MoveCmd{}, m.cmds[0]);
      write_delta_usercmd(w, m.cmds[0], m.cmds[1]);
      write_delta_usercmd(w, m.cmds[1], m.cmds[2]);
      return true;
    }
    case clc::stringcmd:
      write_byte(w, m.id);
      write_string(w, m.text);
      return true;
    case clc::delta:
      write_byte(w, m.id);
      write_byte(w, m.delta);
      return true;
    case clc::tmove:
      write_byte(w, m.id);
      for (int i = 0; i < 3; i++) write_coord(w, m.origin[i]);
      return true;
    case clc::upload:
      write_byte(w, m.id);
      write_short(w, m.size);
      write_byte(w, m.percent);
      if (m.size > 0) gn::write_bytes(w, m.data.data(), m.data.size());
      return true;
    default:
      return false;
  }
}

bool read_clc(gn::Reader& r, ClcMsg& m, std::string& error) {
  m = ClcMsg{};
  m.id = static_cast<uint8_t>(read_byte(r));
  switch (m.id) {
    case clc::nop:
      break;
    case clc::move:
      m.checksum = static_cast<uint8_t>(read_byte(r));
      m.body_at = r.at;
      m.loss = static_cast<uint8_t>(read_byte(r));
      m.cmds[0] = read_delta_usercmd(r, sh::MoveCmd{});
      m.cmds[1] = read_delta_usercmd(r, m.cmds[0]);
      m.cmds[2] = read_delta_usercmd(r, m.cmds[1]);
      m.body_end = r.at;
      break;
    case clc::stringcmd:
      m.text = read_string(r);
      if (m.text.size() > 1023) m.text.resize(1023);
      break;
    case clc::delta:
      m.delta = read_byte(r);
      break;
    case clc::tmove:
      for (int i = 0; i < 3; i++) m.origin[i] = read_coord(r);
      break;
    case clc::upload: {
      m.size = read_short(r);
      m.percent = read_byte(r);
      if (m.size > 0) {
        const uint8_t* p = gn::read_view(r, static_cast<size_t>(m.size));
        if (p) m.data.assign(p, p + m.size);
      }
      break;
    }
    default:
      error = "read_clc: unknown client message id " + std::to_string(m.id) +
              "; next: the client is dropped (the stream cannot be resynced)";
      return false;
  }
  if (r.bad) {
    error = std::string("read_clc: ") + clc_name(m.id) + "'s body runs past the payload's end";
    return false;
  }
  return true;
}

std::string describe_clc(const ClcMsg& m) {
  const char* n = clc_name(m.id);
  std::string s = n ? n : "clc " + std::to_string(m.id);
  switch (m.id) {
    case clc::move:
      s += " checksum " + std::to_string(m.checksum) + " loss " + std::to_string(m.loss);
      for (const sh::MoveCmd& c : m.cmds)
        s += " [" + std::to_string(c.msec) + "ms " + std::to_string(c.forward) + "," +
             std::to_string(c.side) + " b" + std::to_string(c.buttons) + "]";
      break;
    case clc::stringcmd:
      s += " \"" + m.text + "\"";
      break;
    case clc::delta:
      s += " " + std::to_string(m.delta);
      break;
    default:
      break;
  }
  return s;
}

}  // namespace shooters_edge::net

// Tests: the classic protocol's messages (games/shooters_edge/net/classic_svc.hpp and
// classic_delta.hpp): every svc message the table names written, read back and written again to
// the same bytes under the base form and the agreed first tier (and float coordinates), every clc
// message likewise, the move's checksum covering what was sent, entity deltas against a frame
// (unchanged entities cost nothing, a removal, a new entity from its baseline, the number and
// model doubles), the player record's base and extended flag forms, and an unknown id refused.
// tests/golden/shooters_edge_classic_messages.txt holds every message's bytes, written by this
// test under GE_GOLDEN_WRITE=1 (a deliberate change to the wire rewrites it; the PR says why).
//
// Golden rows: <form> <message name> <bytes hex>.
//
// ge:summary Tests: every classic svc and clc message round-tripped, entity and player deltas
// ge:keywords test classic-protocol svc clc round-trip packetentities playerinfo delta golden
// ge:keywords serverdata sound temp-entity download move checksum
// ge:skill net shooter testing
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooters_edge/net/classic_svc.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sh = ge::shooter;

namespace {

constexpr const char* kGolden = "tests/golden/shooters_edge_classic_messages.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

struct Form {
  const char* name;
  cn::WireCtx ctx;
};

std::vector<Form> forms() {
  Form base{"base", {}};
  Form tier{"tier", {}};
  tier.ctx.ext = cn::first_tier();
  tier.ctx.chunked = true;
  Form big{"float", {}};
  big.ctx.ext = cn::first_tier(true);
  big.ctx.ext[cn::ExtFamily::mvd] = cn::kMvdFloatCoords | cn::kMvdTeleport;
  big.ctx.chunked = true;
  return {base, tier, big};
}

cn::EntityState entity(int number, int k) {
  cn::EntityState e;
  e.number = number;
  e.modelindex = 3 + k;
  e.frame = k;
  e.skin = k & 1;
  e.colormap = k % 3;
  e.effects = k & 4;
  e.origin = {12.5f * static_cast<float>(k), -40.0f, 24.125f};
  e.angles = {0, 90.0f, 0};
  return e;
}

// A message of every id, its fields filled by its layout (or by hand for the special ones).
std::vector<cn::SvcMsg> every_svc() {
  std::vector<cn::SvcMsg> out;
  int count = 0;
  const cn::SvcDef* defs = cn::svc_defs(count);
  for (int d = 0; d < count; d++) {
    cn::SvcMsg m;
    m.id = defs[d].id;
    if (defs[d].layout[0] != '*') {
      int k = 1;
      for (const char* c = defs[d].layout; *c; c++, k++) {
        switch (*c) {
          case 'b':
            m.ints.push_back(k * 17 & 0xff);
            break;
          case 'c':
            m.ints.push_back(-k);
            break;
          case 'h':
            m.ints.push_back(-300 * k);
            break;
          case 'l':
            m.ints.push_back(70000 * k);
            break;
          case 's':
            m.strings.push_back("text " + std::to_string(k));
            break;
          default:  // f C O A: values on every form's grid
            m.floats.push_back(static_cast<float>(k) * 45.0f);
            break;
        }
      }
      out.push_back(m);
      continue;
    }
    switch (m.id) {
      case cn::svc::sound:
        m.ints = {37, 2, 200, 32, 9};
        m.floats = {8, -16, 64};
        out.push_back(m);
        m.ints = {37, 2, 255, 64, 9};  // the defaults: no volume or attenuation byte
        break;
      case cn::svc::soundextended:
        m.ints = {9000, 3, 255, 64, 300};
        m.floats = {8, -16, 64};
        break;
      case cn::svc::setangle:
        m.ints = {1};
        m.floats = {0, 180, 0};
        break;
      case cn::svc::serverdata:
        m.ints = {cn::kProtocolVersion, 7, 2};
        m.strings = {"edge", "arena"};
        for (int i = 0; i < 10; i++) m.floats.push_back(static_cast<float>(100 * i));
        break;
      case cn::svc::spawnstatic2:
      case cn::svc::spawnbaseline2:
        m.ints = {12};
        m.ents.push_back({12, 0, entity(12, 2)});
        break;
      case cn::svc::temp_entity:
        m.ints = {cn::te::gunshot, 3};
        m.floats = {1, 2, 3};
        out.push_back(m);
        m.ints = {cn::te::lightning2, 5};
        m.floats = {1, 2, 3, 4, 5, 6};
        break;
      case cn::svc::download:
        m.ints = {4, 50};
        m.data = {1, 2, 3, 4};
        break;
      case cn::svc::playerinfo:
        m.player.slot = 3;
        m.player.origin = {100, -200, 24};
        m.player.frame = 5;
        m.player.has_msec = true;
        m.player.msec = 13;
        m.player.has_command = true;
        m.player.command.angles = {10, 90, 0};
        m.player.command.forward = 400;
        m.player.velocity = {320, 0, -100};
        m.player.onground = true;
        break;
      case cn::svc::voice:  // talker 3, Opus generation 1, sequence 9, one frame of 3 bytes
        m.ints = {3, 0x21, 9};
        m.data = {3, 0, 1, 2, 3};
        break;
      case cn::svc::nails:
        m.ints = {2};
        m.data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        break;
      case cn::svc::packetentities:
      case cn::svc::deltapacketentities:
        m.ints = {5};
        m.ents.push_back({4, 0, entity(4, 1)});
        m.ents.push_back({9, cn::kURemove, {}});
        m.ents.push_back({300, 0, entity(300, 3)});
        break;
      default:  // the lists
        m.ints = {1, 0};
        m.strings = {"progs/player.mdl", "maps/arena.bsp"};
        break;
    }
    out.push_back(m);
  }
  return out;
}

std::vector<uint8_t> written(const cn::SvcMsg& m, const cn::WireCtx& ctx) {
  std::vector<uint8_t> buf(4096);
  gn::Writer w = gn::make_writer(buf.data(), buf.size());
  if (!cn::write_svc(w, m, ctx)) return {};
  buf.resize(w.size);
  return buf;
}

std::string hex_of(const std::vector<uint8_t>& b) { return cn::to_hex(b.data(), b.size()); }

}  // namespace

TEST(every_svc_message_round_trips_to_the_same_bytes_in_every_form) {
  std::vector<std::vector<std::string>> got;
  for (const Form& f : forms()) {
    for (const cn::SvcMsg& m : every_svc()) {
      const cn::SvcDef* d = cn::svc_def(m.id);
      const std::vector<uint8_t> a = written(m, f.ctx);
      CHECK_MSG(!a.empty(), std::string("write_svc refused ") + d->name);
      gn::Reader r = gn::make_reader(a.data(), a.size());
      cn::SvcMsg back;
      std::string error;
      CHECK_MSG(cn::read_svc(r, back, f.ctx, error), std::string(d->name) + ": " + error);
      CHECK_MSG(remaining(r) == 0, std::string(d->name) + " left bytes unread");
      CHECK_MSG(written(back, f.ctx) == a,
                std::string(d->name) + " (" + f.name + ") did not write back to the same bytes");
      CHECK(!cn::describe_svc(back).empty());
      got.push_back({f.name, d->name, hex_of(a)});
    }
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/net/classic_svc.hpp's messages: <form> <name> <bytes hex>, "
           "written by tests/shooters_edge_classic_messages_test.cpp\n";
    for (const auto& row : got) out << row[0] << ' ' << row[1] << ' ' << row[2] << '\n';
    std::printf("wrote %s (%zu rows)\n", kGolden, got.size());
    return;
  }
  const auto rows = ge::test::golden_words(kGolden);
  CHECK_MSG(rows.size() == got.size(), std::string(kGolden) + " has the wrong row count");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++)
    CHECK_MSG(rows[i] == got[i], "the wire moved: " + got[i][0] + " " + got[i][1] + " is " +
                                     got[i][2] +
                                     "; if meant, rewrite with GE_GOLDEN_WRITE=1 "
                                     "and say why in the PR");
}

TEST(the_sound_bytes_follow_the_defaults) {
  cn::SvcMsg m;
  m.id = cn::svc::sound;
  m.ints = {37, 2, 255, 64, 9};
  m.floats = {8, -16, 64};
  // id, packed short, sound byte, three coords
  CHECK(written(m, {}).size() == 1 + 2 + 1 + 6);
  m.ints = {37, 2, 200, 32, 9};
  CHECK(written(m, {}).size() == 1 + 2 + 2 + 1 + 6);
}

TEST(an_unknown_id_is_refused_with_its_place) {
  const uint8_t raw[] = {cn::svc::nop, 200};
  gn::Reader r = gn::make_reader(raw, sizeof raw);
  cn::SvcMsg m;
  std::string error;
  CHECK(cn::read_svc(r, m, {}, error));
  CHECK(!cn::read_svc(r, m, {}, error));
  CHECK(error.find("unknown message id 200") != std::string::npos);
}

TEST(every_clc_message_round_trips) {
  std::vector<cn::ClcMsg> msgs(6);
  msgs[0].id = cn::clc::nop;
  msgs[1].id = cn::clc::move;
  msgs[1].loss = 3;
  msgs[1].cmds[0].msec = 13;
  msgs[1].cmds[1].msec = 13;
  msgs[1].cmds[2].msec = 12;
  msgs[1].cmds[2].angles = {5, 270, 0};
  msgs[1].cmds[2].buttons = 1;
  msgs[2].id = cn::clc::stringcmd;
  msgs[2].text = "say hello";
  msgs[3].id = cn::clc::delta;
  msgs[3].delta = 17;
  msgs[4].id = cn::clc::tmove;
  msgs[4].origin = {1, -2, 3.5f};
  msgs[5].id = cn::clc::upload;
  msgs[5].data = {9, 8, 7};
  msgs[5].size = 3;
  msgs[5].percent = 100;
  for (const cn::ClcMsg& m : msgs) {
    uint8_t buf[256];
    gn::Writer w = gn::make_writer(buf, sizeof buf);
    CHECK(cn::write_clc(w, m));
    gn::Reader r = gn::make_reader(buf, w.size);
    cn::ClcMsg back;
    std::string error;
    CHECK_MSG(cn::read_clc(r, back, error), std::string(cn::clc_name(m.id)) + ": " + error);
    CHECK_MSG(remaining(r) == 0, std::string(cn::clc_name(m.id)) + " left bytes unread");
    uint8_t again[256];
    gn::Writer w2 = gn::make_writer(again, sizeof again);
    CHECK(cn::write_clc(w2, back));
    CHECK_MSG(w2.size == w.size && std::memcmp(buf, again, w.size) == 0,
              std::string(cn::clc_name(m.id)) + " did not write back to the same bytes");
    CHECK(!cn::describe_clc(back).empty());
  }
}

TEST(a_move_carries_the_checksum_of_what_it_sent) {
  sh::MoveCmd cmds[3];
  for (auto& c : cmds) c.msec = 13;
  cmds[2].forward = 400;
  uint8_t buf[64];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  cn::write_move(w, cmds, 5, 1234);
  gn::Reader r = gn::make_reader(buf, w.size);
  cn::ClcMsg m;
  std::string error;
  CHECK(cn::read_clc(r, m, error));
  CHECK(m.loss == 5 && m.cmds[2].forward == 400);
  CHECK(m.checksum == cn::move_checksum(buf + m.body_at, m.body_end - m.body_at, 1234));
  CHECK(m.checksum != cn::move_checksum(buf + m.body_at, m.body_end - m.body_at, 1235));
}

TEST(entity_lists_delta_against_the_frame) {
  const cn::Ext ext = cn::first_tier();
  std::vector<cn::EntityState> baselines(512);
  for (int i = 0; i < 512; i++) baselines[static_cast<size_t>(i)].number = i;
  std::vector<cn::EntityState> from = {entity(4, 1), entity(9, 2), entity(20, 3)};
  for (auto& e : from) e = cn::grid_entity(e, cn::Coords::shorts);
  // Unchanged: the end word alone.
  uint8_t buf[2048];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  cn::write_packet_entities(w, &from, from, baselines, ext);
  CHECK(w.size == 2);
  // 4 moves, 9 goes, 300 is new (a number past 255 and 511 needs the doubles).
  std::vector<cn::EntityState> to = {entity(4, 1), entity(20, 3), entity(300, 4)};
  to[0].origin.x += 8;
  w = gn::make_writer(buf, sizeof buf);
  cn::write_packet_entities(w, &from, to, baselines, ext);
  gn::Reader r = gn::make_reader(buf, w.size);
  std::vector<cn::EntityState> out;
  std::vector<cn::EntityRecord> recs;
  CHECK(cn::read_packet_entities(r, &from, baselines, ext, out, &recs));
  CHECK(remaining(r) == 0);
  CHECK(out.size() == 3);
  CHECK(recs.size() == 3);  // 4's origin, 9's removal, 300 from its baseline (20 unchanged)
  for (size_t i = 0; i < out.size() && i < to.size(); i++) {
    const cn::EntityState want = cn::grid_entity(to[i], cn::Coords::shorts);
    CHECK(out[i].number == want.number && out[i].origin.x == want.origin.x &&
          out[i].modelindex == want.modelindex && out[i].frame == want.frame);
  }
  // A model past 255 under the model double.
  cn::EntityState big = entity(5, 1);
  big.modelindex = 300;
  w = gn::make_writer(buf, sizeof buf);
  CHECK(cn::write_entity_delta(w, baselines[5], big, false, ext));
  r = gn::make_reader(buf, w.size);
  const uint16_t word = gn::read_u16(r);
  const cn::EntityRecord rec = cn::read_entity_record(r, word, baselines[5], ext);
  CHECK(rec.state.modelindex == 300);
  CHECK((rec.bits & cn::kUModelDbl) != 0);
}

TEST(player_records_use_the_base_and_the_extended_flags) {
  cn::PlayerState p;
  p.slot = 1;
  p.origin = {1, 2, 3};
  p.onground = true;
  p.solid = true;
  cn::Ext base;
  base[cn::ExtFamily::z] = cn::kZOnGround | cn::kZSolid;
  const uint32_t f = cn::playerinfo_flags(p, base);
  CHECK((f & cn::kPfOnGround) && (f & cn::kPfSolid));
  cn::Ext x = cn::first_tier();
  p.alpha = 128;
  const uint32_t fx = cn::playerinfo_flags(p, x);
  CHECK((fx & cn::kPfOnGroundX) && (fx & cn::kPfSolidX) && (fx & cn::kPfTrans));
  CHECK(fx & cn::kPfExtra);
  for (const cn::Ext* e : {&base, &x}) {
    uint8_t buf[128];
    gn::Writer w = gn::make_writer(buf, sizeof buf);
    cn::write_playerinfo(w, p, *e);
    gn::Reader r = gn::make_reader(buf, w.size);
    const cn::PlayerState back = cn::read_playerinfo(r, *e);
    CHECK(remaining(r) == 0);
    CHECK(back.onground && back.solid && back.slot == 1 && back.origin.z == 3);
  }
}

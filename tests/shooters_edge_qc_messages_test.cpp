// Tests: what the network builtins write (games/shooters_edge/server/messages.hpp and
// bi_message.cpp): each Write builtin's encoding in its destination's buffer, the signon only
// while loading and msg_entity's client, multicast carrying and clearing its buffer to the
// right recipients, sound's checks and recipients, stuffcmd's line buffering and the drop, the
// prints and their spectator copies, light styles, particles and ambient sounds.
//
// ge:summary Tests: the Write encodings, multicast, sound, stuffcmd, the prints and their copies
// ge:keywords test qc messages writebyte writecoord writeangle writestring multicast sound
// ge:keywords stuffcmd bprint sprint centerprint sv_specprint lightstyle particle ambientsound
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/assembler.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge_qc_fixture.hpp"

using qcfix::ent;
using qcfix::Fixture;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

using Bytes = std::vector<uint8_t>;

Bytes encoded(sv::WriteKind k, float v, const char* text = "") {
  Bytes b;
  sv::encode(b, k, v, text);
  return b;
}

}  // namespace

TEST(each_value_is_encoded_little_endian) {
  CHECK(encoded(sv::WriteKind::byte_, 200) == Bytes{200});
  CHECK(encoded(sv::WriteKind::byte_, 300.7f) == Bytes{44});
  CHECK(encoded(sv::WriteKind::char_, -1) == Bytes{0xff});
  CHECK(encoded(sv::WriteKind::short_, -2) == (Bytes{0xfe, 0xff}));
  CHECK(encoded(sv::WriteKind::long_, 65536 + 2) == (Bytes{2, 0, 1, 0}));
  CHECK(encoded(sv::WriteKind::coord, 1.5f) == (Bytes{12, 0}));
  CHECK(encoded(sv::WriteKind::coord, -0.1f) == (Bytes{0, 0}));  // truncated toward zero
  CHECK(encoded(sv::WriteKind::angle, 90) == Bytes{64});
  CHECK(encoded(sv::WriteKind::angle, 359) == Bytes{255});
  CHECK(encoded(sv::WriteKind::string, 0, "hi") == (Bytes{'h', 'i', 0}));
  CHECK(encoded(sv::WriteKind::entity, 300) == (Bytes{44, 1}));
}

TEST(the_write_builtins_fill_their_destinations) {
  Fixture fx;
  const int p = fx.join(0, "one");
  fx.call(52, {0, 7});
  fx.call(53, {2, -3});
  fx.call(54, {2, 513});
  fx.call(55, {2, 1});
  fx.call(56, {2, 2});
  fx.call(57, {2, 180});
  fx.call(58, {2, "ok"});
  fx.call(59, {2, ent(p)});
  CHECK(fx.s->out.unreliable == Bytes{7});
  CHECK(fx.s->out.all == (Bytes{0xfd, 1, 2, 1, 0, 0, 0, 16, 0, 128, 'o', 'k', 0, 1, 0}));
  // One client: the one in msg_entity.
  pg::set_global_int(fx.vm(), pg::kGMsgEntity, pg::entity_value(fx.vm(), p));
  fx.s->sv.g.self = 0;
  fx.call(52, {1, 9});
  CHECK(fx.s->out.one[0] == Bytes{9});
  // The signon only while the map loads; a bad destination faults.
  fx.call(52, {3, 1});
  CHECK(fx.fault);
  fx.s->loading = true;
  fx.call(52, {3, 1});
  CHECK(!fx.fault && fx.s->out.init.size() == 1);
  fx.call(52, {9, 1});
  CHECK(fx.fault);
}

TEST(multicast_sends_its_buffer_and_clears_it) {
  Fixture fx;
  fx.join(0, "one");
  fx.join(1, "two");
  fx.call(52, {4, 3});
  fx.call(56, {4, 10});
  fx.call(82, {sh::Vec3f{0, 0, 24}, 1});
  const auto ev = fx.events(sv::EventKind::multicast);
  CHECK(ev.size() == 1);
  CHECK(ev.size() == 1 && ev[0]->bytes == (Bytes{3, 80, 0}));
  CHECK(ev.size() == 1 && ev[0]->to == 3u && !ev[0]->reliable);
  CHECK(fx.s->out.multicast.empty());
  fx.call(82, {sh::Vec3f{0, 0, 0}, 4});  // reliable PHS
  CHECK(fx.events(sv::EventKind::multicast).back()->reliable);
  fx.call(82, {sh::Vec3f{0, 0, 0}, 6});
  CHECK(fx.fault);
}

TEST(sound_checks_its_arguments_and_reaches_who_hears) {
  Fixture fx;
  const int p = fx.join(0, "one");
  fx.join(1, "two");
  fx.call(8, {ent(p), 1, "weapons/rocket.wav", 1, 1});
  auto ev = fx.events(sv::EventKind::sound);
  CHECK(ev.size() == 1);
  CHECK(ev.size() == 1 && ev[0]->values[0] == 255.0f && ev[0]->level == 1 && ev[0]->ent == p);
  CHECK(ev.size() == 1 && ev[0]->to == 3u);  // no sets yet: everyone in every set
  fx.call(8, {ent(p), 9, "weapons/rocket.wav", 1, 1});
  ev = fx.events(sv::EventKind::sound);
  CHECK(ev.back()->reliable && ev.back()->level == 1);
  fx.call(8, {ent(p), 1, "weapons/rocket.wav", 2, 1});
  CHECK(fx.fault);
  fx.call(8, {ent(p), 1, "weapons/rocket.wav", 1, 5});
  CHECK(fx.fault);
  fx.call(8, {ent(p), 16, "weapons/rocket.wav", 1, 1});
  CHECK(fx.fault);
  const size_t before = fx.events(sv::EventKind::sound).size();
  fx.call(8, {ent(p), 1, "never/precached.wav", 1, 1});
  CHECK(!fx.fault && fx.events(sv::EventKind::sound).size() == before);
  CHECK(fx.s->console.back() == "no precache: never/precached.wav");
}

TEST(the_hearing_set_limits_a_sound_when_the_map_has_one) {
  Fixture fx;
  fx.join(0, "near");
  fx.join(1, "far");
  // A map whose hearing set holds nothing: only clients within 1024 of the sound hear it.
  fx.s->vis = [](void*, sh::Vec3f, sh::Vec3f, bool) { return false; };
  fx.call(8, {ent(1), 1, "weapons/rocket.wav", 1, 1});
  const auto ev = fx.events(sv::EventKind::sound);
  CHECK(ev.size() == 1 && (ev[0]->to & 1u) != 0);
}

TEST(stuffcmd_buffers_to_a_newline_and_disconnect_drops) {
  Fixture fx;
  const int p = fx.join(0, "one");
  fx.call(21, {ent(p), "bind x "});
  CHECK(fx.events(sv::EventKind::stuffcmd).empty());
  fx.call(21, {ent(p), "\"say hi\"\n"});
  const auto ev = fx.events(sv::EventKind::stuffcmd);
  CHECK(ev.size() == 1 && ev[0]->text == "bind x \"say hi\"\n" && ev[0]->to == 1u);
  fx.call(21, {ent(40), "x\n"});
  CHECK(fx.fault);
  fx.call(21, {ent(p), std::string(300, 'a')});
  CHECK(fx.fault);
  fx.s->clients[0].stuff.clear();
  fx.call(21, {ent(p), "disconnect\n"});
  CHECK(fx.events(sv::EventKind::drop).size() == 1);
  sv::qc_frame(*fx.s, nullptr, 0);
  CHECK(!fx.s->clients[0].active);
}

TEST(the_prints_and_their_spectator_copies) {
  Fixture fx;
  const int p = fx.join(0, "player");
  const int spec = fx.join(1, "watcher", true);
  fx.call(23, {2, "all ", "of ", "you\n"});
  auto ev = fx.events(sv::EventKind::print);
  CHECK(ev.back()->text == "all of you\n" && ev.back()->level == 2 && ev.back()->to == 3u);
  fx.call(24, {ent(p), 1, "just ", "you\n"});
  ev = fx.events(sv::EventKind::print);
  CHECK(ev.back()->text == "just you\n" && ev.back()->to == 1u);
  fx.call(24, {ent(0), 1, "nobody\n"});
  CHECK(fx.s->console.back() == "tried to sprint to a non-client");
  fx.call(25, {"to the ", "console"});
  CHECK(fx.s->console.back() == "to the console");
  fx.call(73, {ent(p), "centre"});
  CHECK(fx.events(sv::EventKind::centerprint).back()->to == 1u);
  // The test mod has no trackent field: no copies whatever is set.
  CHECK(fx.field("trackent") < 0);
  sv::server_command(*fx.s, "sv_specprint 3");
  sv::set_user_info(*fx.s, spec - 1, "sp", "3");
  fx.call(73, {ent(p), "centre"});
  CHECK(fx.events(sv::EventKind::centerprint).back()->to == 1u);
}

TEST(a_spectator_tracking_a_player_gets_the_copies_both_ask_for) {
  pg::Progs mod;
  {
    pg::Assembler a;
    a.field("trackent", pg::kTypeEntity);
    std::string error;
    CHECK_MSG(a.build(mod, error), error);
  }
  Fixture fx("{ \"classname\" \"worldspawn\" }", {}, &mod);
  CHECK_MSG(fx.loaded, fx.error);
  const int p = fx.join(0, "player");
  const int spec = fx.join(1, "watcher", true);
  *pg::entity_field(fx.vm(), spec, fx.field("trackent")) =
      static_cast<uint32_t>(pg::entity_value(fx.vm(), p));
  fx.call(73, {ent(p), "centre"});
  CHECK(fx.events(sv::EventKind::centerprint).back()->to == 1u);  // the server does not ask
  sv::server_command(*fx.s, "sv_specprint 1");
  fx.call(73, {ent(p), "centre"});
  CHECK(fx.events(sv::EventKind::centerprint).back()->to == 1u);  // nor does he
  sv::set_user_info(*fx.s, spec - 1, "sp", "1");
  fx.call(73, {ent(p), "centre"});
  CHECK(fx.events(sv::EventKind::centerprint).back()->to == 3u);
  fx.call(24, {ent(p), 1, "sprint\n"});
  CHECK(fx.events(sv::EventKind::print).back()->to == 1u);  // bit 2 is off on the server
  sv::server_command(*fx.s, "sv_specprint 3");
  sv::set_user_info(*fx.s, spec - 1, "sp", "3");
  fx.call(24, {ent(p), 1, "sprint\n"});
  CHECK(fx.events(sv::EventKind::print).back()->to == 3u);
}

TEST(light_styles_particles_and_ambient_sounds) {
  Fixture fx;
  fx.join(0, "one");
  CHECK(fx.s->lightstyles[0] == "m");  // the test mod's worldspawn
  fx.call(35, {5, "abcz"});
  CHECK(fx.s->lightstyles[5] == "abcz");
  CHECK(fx.events(sv::EventKind::lightstyle).size() == 1);
  fx.call(35, {64, "a"});
  CHECK(fx.fault);
  fx.call(48, {sh::Vec3f{1, 2, 3}, sh::Vec3f{0, 0, 1}, 0, 255});
  CHECK(fx.events(sv::EventKind::particle).back()->text == "explosion");
  fx.call(48, {sh::Vec3f{1, 2, 3}, sh::Vec3f{0, 0, 1}, 73, 20});
  CHECK(fx.events(sv::EventKind::particle).back()->text == "blood");
  const size_t n = fx.events(sv::EventKind::particle).size();
  fx.call(48, {sh::Vec3f{1, 2, 3}, sh::Vec3f{0, 0, 1}, 12, 20});
  CHECK(fx.events(sv::EventKind::particle).size() == n);  // nothing the protocol carries
  fx.call(74, {sh::Vec3f{0, 0, 0}, "weapons/explode.wav", 0.5f, 1});
  const auto amb = fx.events(sv::EventKind::ambient);
  CHECK(amb.size() == 1 && amb[0]->values[0] == 127.0f && amb[0]->values[1] == 64.0f);
}

TEST(clear_frame_keeps_the_reliable_streams) {
  Fixture fx;
  fx.call(52, {0, 1});
  fx.call(52, {2, 1});
  fx.call(23, {1, "x"});
  sv::clear_frame(fx.s->out);
  CHECK(fx.s->out.unreliable.empty() && fx.s->out.events.empty());
  CHECK(fx.s->out.all.size() == 1);
  CHECK(fx.s->out.events_total == 1);
}

// Tests: recording a multiplayer game with its voices (games/shooters_edge/net/classic_record.hpp)
// over engine/net's simulated network: the voice messages round trip; four scripted clients on two
// teams talk in turn on a server that relays voice by team; one client records in the voice format
// (`.qwdv`) and another plainly, the server records the multiview voice format (`.mvdv`); then
// each recording is read back by the demo reader (client/demo.hpp) to its end: the client's holds
// its own voice and its team-mate's and nobody else's, the plain one holds none, the server's holds
// every talker's, and a second session records the same bytes. Packed as they go, the three
// recordings unpack to the plain ones' bytes and play the same; a recording plays out of every
// archive (gzip, zstd, xz, tar.gz, zip, 7z), a readme beside it in the archive ignored.
//
// ge:summary Tests: client and multiview recordings with voice, by team, read back to the end
// ge:keywords test record recorder voice qwdv mvdv team-voice client-recording multiview
// ge:keywords voice-relay demo-reader packed zstd archive 7z zip tar-gz qwdz mvdz
// ge:skill net shooter testing
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "audio/synth.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_voice.hpp"
#include "shooters_edge/client/record_cvars.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_record.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "vfs/lzma.hpp"
#include "vfs/tar.hpp"
#include "vfs/unpack.hpp"
#include "vfs/zip.hpp"
#include "vfs/zstd.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;
namespace sv = shooters_edge::server;

namespace {

constexpr int kClients = 4;
constexpr int kTicks = 1300;  // 13 ms each: about 17 s

// Client i (seed i + 1) talks for 150 ticks from tick 300 * (i + 1); the last one never.
bool talk(int tick, int seed) {
  return seed < kClients && tick >= 300 * seed && tick < 300 * seed + 150;
}

struct Session {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  gn::SimNetwork net;
  gn::Transport st, ct[kClients];
  cn::ClassicServer server;
  cn::ClassicClientEnd c[kClients];
  cn::ClientRecorder voiced, plain;  // client 0's (voice format) and client 1's (plain)
  cn::MultiviewRecorder mvd;
  int64_t now = 0;
  bool packed = false;  // every recorder packs as it goes

  bool run() {
    std::string error;
    if (!sv::build_test_mod(progs, error)) return false;
    sv::QcServerOptions so;
    so.loop.max_edicts = 1024;
    qc = sv::make_qc_server(progs, so, error);
    if (!qc) return false;
    sv::MapSpec map;
    map.name = "arena";
    map.models = sv::arena_models();
    map.entities = sv::kArenaEntities;
    if (!sv::load_map(*qc, map, error)) return false;
    gn::LinkConditions lan;
    gn::conditions_preset("lan", lan);
    net = gn::make_sim_network(1, lan);
    gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
    cn::ServerConfig cfg;
    cfg.voice = true;
    if (!cn::classic_start(server, *qc, st, cfg, error)) return false;
    cn::mvd_start(mvd, true, packed ? cl::kDefaultPackLevel : 0);
    server.mvd = &mvd;
    for (int i = 0; i < kClients; i++) {
      gn::open_sim_transport(ct[i], net,
                             gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
      cn::ClientConfig cc;
      cc.name = "p" + std::to_string(i);
      cc.team = i < 2 ? "red" : "blue";
      cc.qport = static_cast<uint16_t>(100 + i);
      cc.seed = i + 1;
      cc.voice = true;
      cc.talk = &talk;
      cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
    }
    cn::record_start(voiced, true, packed ? cl::kDefaultPackLevel : 0);
    cn::record_start(plain, false, packed ? cl::kDefaultPackLevel : 0);
    c[0].record = &voiced;
    c[1].record = &plain;
    for (int f = 0; f < kTicks; f++) {
      now += 13000;
      gn::sim_advance(net, now);
      cn::classic_frame(server, now);
      for (cn::ClassicClientEnd& k : c) cn::client_tick(k, now);
    }
    cn::mvd_stop(mvd);
    cn::record_stop(voiced);
    cn::record_stop(plain);
    return true;
  }
};

struct Read {
  cl::DemoFormat format = cl::DemoFormat::single;
  bool voice_format = false;
  int64_t frames = 0, voice_messages = 0;
  std::set<int> talkers;
  int most_players = 0;
  std::string error;
  bool ended = false;
};

Read read_back(std::vector<uint8_t> bytes) {
  Read r;
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, r.error)) return r;
  r.format = d.state.format;
  r.voice_format = d.state.voice_format;
  for (;;) {
    const cl::DemoRead st = cl::next_frame(d, r.error);
    if (st == cl::DemoRead::error) return r;
    if (st == cl::DemoRead::end) break;
    r.frames++;
    for (const cl::DemoVoice& v : d.state.voice) {
      r.voice_messages++;
      r.talkers.insert(v.sender);
    }
    int players = 0;
    for (const cl::DemoEntity& p : d.state.players) players += p.present ? 1 : 0;
    r.most_players = std::max(r.most_players, players);
  }
  r.ended = true;
  return r;
}

}  // namespace

TEST(voice_messages_round_trip) {
  cn::SvcMsg v;
  v.id = cn::svc::voice;
  v.ints = {5, cl::kVoiceOpus << 4 | 3, 200};
  v.data = {3, 0, 'a', 'b', 'c'};
  uint8_t buf[64];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  CHECK(cn::write_svc(w, v, cn::WireCtx{}));
  gn::Reader r = gn::make_reader(buf, w.size);
  cn::SvcMsg back;
  std::string error;
  CHECK_MSG(cn::read_svc(r, back, cn::WireCtx{}, error), error);
  CHECK(back.ints == v.ints && back.data == v.data);
  cn::ClcMsg u;
  u.id = cn::clc::voice;
  u.gen = 0x21;
  u.seq = 7;
  u.data = {1, 0, 9};
  w = gn::make_writer(buf, sizeof buf);
  CHECK(cn::write_clc(w, u));
  r = gn::make_reader(buf, w.size);
  cn::ClcMsg uback;
  CHECK_MSG(cn::read_clc(r, uback, error), error);
  CHECK(uback.gen == 0x21 && uback.seq == 7 && uback.data == u.data);
  // The demo reader's view of the same message, its frames split.
  gn::Writer w2 = gn::make_writer(buf, sizeof buf);
  cn::write_svc(w2, v, cn::WireCtx{});
  cl::DemoMsg dm{std::span<const uint8_t>(buf + 1, w2.size - 1)};
  cl::DemoVoice dv;
  CHECK(cl::read_voice(dm, dv));
  std::vector<std::span<const uint8_t>> frames;
  CHECK(cl::voice_frames(dv, frames));
  CHECK(dv.sender == 5 && dv.seq == 200 && frames.size() == 1 && frames[0].size() == 3);
}

TEST(a_team_session_records_its_voices_in_the_voice_formats) {
  auto owned = std::make_unique<Session>();
  Session& s = *owned;
  CHECK(s.run());
  int slot[kClients];
  for (int i = 0; i < kClients; i++) {
    CHECK_MSG(s.c[i].stage == cn::ClientStage::active,
              "client " + std::to_string(i) + " " + cn::client_stage_name(s.c[i].stage));
    slot[i] = s.c[i].playernum;
  }
  // The relay: each team hears its own, never the other.
  CHECK(s.server.counts.voice_in > 0 && s.server.counts.voice_refused == 0);
  CHECK(s.c[0].voice_sent > 0 && s.c[2].voice_sent > 0 && s.c[3].voice_sent == 0);
  CHECK(s.c[0].voice_heard[slot[1]] > 0 && s.c[1].voice_heard[slot[0]] > 0);
  CHECK(s.c[0].voice_heard[slot[2]] == 0 && s.c[2].voice_heard[slot[0]] == 0);
  CHECK(s.c[3].voice_heard[slot[2]] > 0);
  CHECK(s.c[0].voice_heard[slot[0]] == 0);  // nobody gets his own back

  // Client 0's `.qwdv`: its own voice and its team-mate's.
  const Read q = read_back(s.voiced.bytes);
  CHECK_MSG(q.ended, q.error);
  CHECK(q.format == cl::DemoFormat::client && q.voice_format);
  CHECK(q.frames > 500);
  CHECK(q.talkers == (std::set<int>{slot[0], slot[1]}));
  CHECK(s.voiced.own_messages > 0 && s.voiced.voice_messages > 0);
  // Client 1's plain recording: no voice, no header, every frame still read.
  const Read p = read_back(s.plain.bytes);
  CHECK_MSG(p.ended, p.error);
  CHECK(p.format == cl::DemoFormat::client && !p.voice_format);
  CHECK(p.voice_messages == 0 && s.plain.stripped > 0);
  // The server's `.mvdv`: every talker, every player seen.
  const Read m = read_back(s.mvd.bytes);
  CHECK_MSG(m.ended, m.error);
  CHECK(m.format == cl::DemoFormat::multiview && m.voice_format);
  CHECK(m.talkers == (std::set<int>{slot[0], slot[1], slot[2]}));
  CHECK(m.most_players == kClients);
  CHECK(m.voice_messages == s.mvd.voice_messages);
  CHECK(m.frames > 1000);

  // The recording's voice decodes: every frame fed to its talker's stream.
  cl::Demo d;
  std::string error;
  CHECK(cl::open_demo(s.mvd.bytes, d, error));
  ge::audio::Synth synth(1);
  cl::DemoVoices voices(synth);
  while (cl::next_frame(d, error) == cl::DemoRead::frame)
    for (const cl::DemoVoice& v : d.state.voice) voices.play(v);
  CHECK(voices.stats().bad == 0);
  CHECK(voices.stats().frames == s.c[0].voice_sent + s.c[1].voice_sent + s.c[2].voice_sent);
}

TEST(a_session_records_the_same_bytes_twice) {
  auto a = std::make_unique<Session>();
  auto b = std::make_unique<Session>();
  CHECK(a->run() && b->run());
  CHECK(a->voiced.bytes == b->voiced.bytes);
  CHECK(a->mvd.bytes == b->mvd.bytes);
}

TEST(a_packed_session_unpacks_to_the_plain_bytes_and_plays) {
  auto a = std::make_unique<Session>();
  auto b = std::make_unique<Session>();
  b->packed = true;
  CHECK(a->run() && b->run());
  const std::pair<const std::vector<uint8_t>*, const std::vector<uint8_t>*> pairs[] = {
      {&a->voiced.bytes, &b->voiced.bytes},
      {&a->plain.bytes, &b->plain.bytes},
      {&a->mvd.bytes, &b->mvd.bytes}};
  for (const auto& [plain, packed] : pairs) {
    CHECK(ge::vfs::is_zstd(*packed));
    CHECK_MSG(packed->size() * 2 < plain->size(),
              std::to_string(packed->size()) + " of " + std::to_string(plain->size()));
    std::vector<uint8_t> back;
    bool cut = true;
    std::string error;
    CHECK_MSG(ge::vfs::zstd_unpack(*packed, back, cut, error), error);
    CHECK(!cut);
    CHECK(back == *plain);
    const Read r = read_back(*packed), want = read_back(*plain);
    CHECK_MSG(r.ended, r.error);
    CHECK(r.format == want.format && r.voice_format == want.voice_format);
    CHECK(r.frames == want.frames && r.voice_messages == want.voice_messages);
  }
  CHECK(b->mvd.plain == static_cast<int64_t>(a->mvd.bytes.size()));
}

TEST(a_recording_plays_out_of_every_archive) {
  auto a = std::make_unique<Session>();
  CHECK(a->run());
  const std::vector<uint8_t>& mvdv = a->mvd.bytes;
  const Read want = read_back(mvdv);
  const auto tar =
      ge::vfs::write_tar({{"notes.txt", {'h', 'i'}}, {"shot.png", {1}}, {"match.mvdv", mvdv}});
  const std::vector<std::vector<uint8_t>> files = {
      ge::vfs::write_gzip(mvdv, "match.mvdv"),
      ge::vfs::zstd_pack(mvdv),
      ge::vfs::xz_pack(mvdv, 6),
      ge::vfs::write_gzip(tar),
      ge::vfs::write_stored_zip({{"readme.txt", {'x'}}, {"demos/MATCH.MVDV", mvdv}}),
      ge::vfs::write_7z("match.mvdv", mvdv),
  };
  for (const auto& f : files) {
    const Read r = read_back(f);
    CHECK_MSG(r.ended, r.error);
    CHECK(r.frames == want.frames && r.voice_messages == want.voice_messages);
  }
  cl::Demo d;
  std::string error;
  CHECK(cl::open_demo(ge::vfs::write_gzip(tar), d, error));
  CHECK(d.inner_name == "match.mvdv");
  CHECK((d.layers == std::vector{ge::vfs::Wrapping::gzip, ge::vfs::Wrapping::tar}));
  // Two recordings in one archive: refused, naming them.
  const auto two = ge::vfs::write_7z("a.mvd", mvdv);
  const auto both = ge::vfs::write_tar({{"a.mvdv", mvdv}, {"b.qwdvz", mvdv}});
  CHECK(cl::open_demo(two, d, error));
  CHECK(!cl::open_demo(both, d, error));
  CHECK_MSG(error.find("a.mvdv, b.qwdvz") != std::string::npos, error);
}

TEST(the_demo_pack_cvars_default_to_packed) {
  ge::console::Console con;
  cl::RecordSettings rs;
  cl::bind_record_cvars(con, rs);
  CHECK(cl::record_pack_level(rs) == cl::kDefaultPackLevel);
  con.execute("demo_pack 0");
  CHECK(cl::record_pack_level(rs) == 0);
  con.execute("demo_pack 1; demo_pack_level 3");
  CHECK(cl::record_pack_level(rs) == 3);
}

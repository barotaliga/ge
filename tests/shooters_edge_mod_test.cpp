// SHOOTER'S EDGE HOSTS A C++ MOD (games/shooters_edge/sim/mod_host.hpp): the sample mod
// `rocket-bounce`, built as a package by this tree and loaded through engine/mod, changes the
// scripted match (its rockets fly half as fast again, a grenade's third bounce lights its fuse),
// and the match with it replays to the bit, frame by frame. A snapshot of the server, the game's
// program and the mod's run taken mid-match and restored plays the same frames again (a seek). A
// hook table shorter than this game's is read only as far as its size; a service called with a bad
// entity or field is refused and counted, never a crash; the mod's stream is the match's seed.
//
// ge:summary Tests: a C++ mod changes Shooter's Edge's match, replays to the bit, survives a seek
// ge:keywords test mod mod-host rocket-bounce replay snapshot seek keyframe hooks refuse random
// ge:skill mod shooter testing
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "mod/loader.hpp"
#include "shooter/frame.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/sim/match.hpp"
#include "shooters_edge/sim/mod_host.hpp"

namespace se = shooters_edge::sim;
namespace sh = ge::shooter;
namespace gm = ge::mod;

namespace {

const ge::mod::LoadedMod& sample() {
  static const std::unique_ptr<gm::LoadedMod> mod = [] {
    auto m = std::make_unique<gm::LoadedMod>();
    const gm::LoadResult r =
        gm::load_mod(std::string(GE_MOD_DIR) + "/rocket-bounce", {SE_MOD_GAME, SE_MOD_API}, *m);
    for (const std::string& e : r.errors) std::printf("  %s\n", e.c_str());
    return m;
  }();
  return *mod;
}

// The speed of a match's first rocket over its first two recorded frames, units/s.
float first_rocket_speed(const se::MatchReport& r) {
  for (const se::MissileTrack& m : r.missiles)
    if (m.rocket && m.points.size() >= 3)
      return sh::length(m.points[2] - m.points[1]) / static_cast<float>(se::kServerFrameMs * 0.001);
  return 0;
}

int count(const se::MatchReport& r, se::EventKind k) {
  int n = 0;
  for (const se::GameEvent& e : r.events) n += e.kind == k ? 1 : 0;
  return n;
}

// A match driven here frame by frame, so it can be snapshotted: the native program with a mod on
// the seam, both players holding attack (player 1 rockets ahead, player 2 grenades up the room).
struct Sim {
  se::Arena arena;
  std::vector<sh::BrushModel> models;
  se::Rules rules;
  se::ModHost mod;
  sh::Server sv;
  uint32_t sequence = 0;
};

std::unique_ptr<Sim> make_sim(const ge_mod_v1* table, uint32_t seed) {
  auto s = std::make_unique<Sim>();
  se::build_arena(s->arena);
  s->models = s->arena.models();
  sh::ServerSettings set;
  set.maxclients = 2;
  set.max_edicts = 64;
  se::create_rules(s->rules, s->arena, set.max_edicts);
  const std::vector<std::string> errors =
      se::create_mod_host(s->mod, table, se::rules_program(s->rules), seed);
  CHECK(errors.empty());
  sh::create_server(s->sv, set, s->models, se::mod_program(s->mod));
  se::spawn_arena_entities(s->sv, s->rules);
  se::mod_match_start(s->mod, s->sv);
  sh::connect_client(s->sv, 0, false, true);
  sh::connect_client(s->sv, 1, false, true);
  return s;
}

uint64_t frame(Sim& s) {
  s.sequence++;
  sh::Packet p[2];
  const float pitch[2] = {0, -30}, yaw[2] = {s.arena.spawn_yaw[0], s.arena.spawn_yaw[1]};
  for (int c = 0; c < 2; c++) {
    p[c].client = c;
    p[c].sequence = s.sequence;
    sh::MoveCmd cmd;
    cmd.msec = static_cast<uint8_t>(se::kServerFrameMs);
    cmd.angles = {pitch[c], yaw[c], 0};
    cmd.buttons = sh::kButtonAttack;
    for (sh::MoveCmd& m : p[c].cmds) m = cmd;
  }
  sh::server_frame(s.sv, p, 2);
  return se::mod_hash(s.mod.run, sh::server_hash(s.sv));
}

// An in-process mod for the hooks' and services' tests: counts each hook in its state.
struct Counts {
  int32_t start_frame, pre, post, call;
  double draws[4];
  int32_t drawn;
};

void count_start(void* st, const ge_mod_host_v1* h) {
  Counts& c = *static_cast<Counts*>(st);
  c.start_frame++;
  if (c.drawn < 4) c.draws[c.drawn++] = h->random(h->ctx);
}
void count_pre(void* st, const ge_mod_host_v1*, int32_t) { static_cast<Counts*>(st)->pre++; }
void count_post(void* st, const ge_mod_host_v1*, int32_t) { static_cast<Counts*>(st)->post++; }
void count_call(void* st, const ge_mod_host_v1*, uint32_t, int32_t, int32_t) {
  static_cast<Counts*>(st)->call++;
}
void misbehave(void* st, const ge_mod_host_v1* h) {
  count_start(st, h);
  const auto* g = static_cast<const se_mod_services_v1*>(h->game);
  (void)g->get_float(h->ctx, 9999, SE_FIELD_HEALTH);  // out of range
  const float v[3] = {1, 2, 3};
  g->set_vec(h->ctx, 1, SE_FIELD_MINS, v);       // the box goes through set_size
  g->remove(h->ctx, 0);                          // the world
  (void)g->get_int(h->ctx, 1, SE_FIELD_HEALTH);  // a float field read as an int
}

se_mod_hooks_v1 counting_hooks() {
  return {sizeof(se_mod_hooks_v1),
          SE_MOD_API,
          nullptr,
          count_start,
          count_pre,
          count_post,
          nullptr,
          nullptr,
          nullptr,
          count_call};
}

ge_mod_v1 table_of(const se_mod_hooks_v1* hooks) {
  return {sizeof(ge_mod_v1), GE_MOD_ABI,     "counts", "1.0.0", SE_MOD_GAME,
          SE_MOD_API,        sizeof(Counts), nullptr,  hooks};
}

const Counts& counts_of(const Sim& s) {
  return *reinterpret_cast<const Counts*>(s.mod.run.state.data());
}

}  // namespace

TEST(the_sample_mod_changes_the_match) {
  if (!sample().table) {
    CHECK(sample().table != nullptr);
    return;
  }
  se::MatchOptions plain;
  plain.frames = 700;
  se::MatchOptions modded = plain;
  modded.mod = sample().table;
  const se::MatchReport a = se::run_match(plain);
  const se::MatchReport b = se::run_match(modded);
  CHECK(b.mod_errors.empty());
  const float va = first_rocket_speed(a), vb = first_rocket_speed(b);
  std::printf("  first rocket %.1f u/s plain, %.1f u/s modded\n", va, vb);
  CHECK(va > 900 && va < 1100);
  CHECK(vb > 1.4f * va && vb < 1.6f * va);
  // The grenade bounced at least three times without the mod; with it the third bounce lit the
  // fuse, so it bounced exactly three times and exploded before its own fuse ran out.
  CHECK(count(a, se::EventKind::grenade_bounce) >= 3);
  CHECK(count(b, se::EventKind::grenade_bounce) == 3);
  bool lit = false;
  for (const std::string& l : b.mod_log) lit = lit || l.find("third bounce") != std::string::npos;
  CHECK(lit);
  CHECK(b.mod_run.refusals == 0);
  CHECK(b.mod_run.hook_calls > 0);
  CHECK(a.final_hash != b.final_hash);
}

TEST(the_match_with_the_mod_replays_to_the_bit) {
  if (!sample().table) return;
  se::MatchOptions opt;
  opt.frames = 700;
  opt.mod = sample().table;
  const se::MatchReport a = se::run_match(opt);
  const se::MatchReport b = se::run_match(opt);
  CHECK(a.frames.size() == b.frames.size());
  size_t differ = 0;
  for (size_t i = 0; i < a.frames.size() && i < b.frames.size(); i++)
    differ += a.frames[i].hash != b.frames[i].hash ? 1 : 0;
  CHECK(differ == 0);
  CHECK(a.mod_run.state == b.mod_run.state);
}

TEST(a_seek_from_a_snapshot_plays_the_same_frames) {
  if (!sample().table) return;
  const std::unique_ptr<Sim> s = make_sim(sample().table, 5);
  for (int i = 0; i < 150; i++) frame(*s);
  // The keyframe: the server, the game's program and the mod's run, as values.
  const sh::Server sv = s->sv;
  const se::Rules rules = s->rules;
  const se::ModRun run = s->mod.run;
  const uint32_t sequence = s->sequence;
  std::vector<uint64_t> first;
  first.reserve(300);
  for (int i = 0; i < 300; i++) first.push_back(frame(*s));
  s->sv = sv;
  s->rules = rules;
  s->mod.run = run;
  s->sequence = sequence;
  std::vector<uint64_t> again;
  again.reserve(300);
  for (int i = 0; i < 300; i++) again.push_back(frame(*s));
  CHECK(first == again);
  CHECK(s->mod.run.refusals == 0);
  CHECK(s->mod.run.hook_calls > 0);
}

TEST(an_older_hook_table_is_read_only_as_far_as_its_size) {
  se_mod_hooks_v1 hooks = counting_hooks();
  hooks.size = static_cast<uint32_t>(offsetof(se_mod_hooks_v1, player_post_think));
  const ge_mod_v1 table = table_of(&hooks);
  const std::unique_ptr<Sim> s = make_sim(&table, 1);
  for (int i = 0; i < 40; i++) frame(*s);
  const Counts& c = counts_of(*s);
  CHECK(c.start_frame > 0);
  CHECK(c.pre > 0);
  CHECK(c.post == 0);
  CHECK(c.call == 0);
}

TEST(a_bad_service_call_is_refused_and_counted) {
  se_mod_hooks_v1 hooks = counting_hooks();
  hooks.start_frame = misbehave;
  const ge_mod_v1 table = table_of(&hooks);
  const std::unique_ptr<Sim> s = make_sim(&table, 1);
  for (int i = 0; i < 10; i++) frame(*s);
  const Counts& c = counts_of(*s);
  CHECK(c.start_frame > 0);
  CHECK(s->mod.run.refusals == 4 * static_cast<int64_t>(c.start_frame));
  CHECK(!s->mod.log.empty() && s->mod.log.front().find("refused get_float") == 0);
  CHECK(!s->sv.edicts[0].free);
}

TEST(the_mods_stream_is_the_matchs_seed) {
  se_mod_hooks_v1 hooks = counting_hooks();
  const ge_mod_v1 table = table_of(&hooks);
  const std::unique_ptr<Sim> a = make_sim(&table, 3), b = make_sim(&table, 3),
                             c = make_sim(&table, 4);
  for (int i = 0; i < 6; i++) {
    frame(*a);
    frame(*b);
    frame(*c);
  }
  CHECK(counts_of(*a).drawn == 4);
  for (int i = 0; i < 4; i++) CHECK_BITS(counts_of(*a).draws[i], counts_of(*b).draws[i]);
  CHECK(counts_of(*a).draws[0] != counts_of(*c).draws[0]);
}

TEST(hooks_of_another_api_are_refused) {
  se_mod_hooks_v1 hooks = counting_hooks();
  hooks.api = SE_MOD_API + 1;
  const ge_mod_v1 table = table_of(&hooks);
  const std::vector<std::string> e = se::check_hooks(&table);
  CHECK(e.size() == 1 && e[0].find("next:") != std::string::npos);
  se::MatchOptions opt;
  opt.frames = 10;
  opt.mod = &table;
  CHECK(!se::run_match(opt).mod_errors.empty());
}

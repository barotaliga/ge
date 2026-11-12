// Tests: our TV node's classic face (games/shooters_edge/net/classic_tv.hpp) and the match it
// keeps for late viewers (classic_mv_state.hpp). A record waits for its bytes and a damaged one is
// refused; a viewer who joins a written match at any record and is sent its catch-up then the rest
// ends where a viewer of the whole match ends (every player, his frags, name and stats, every
// entity); names are cleaned and kept apart. Then over loopback TCP, a made-up server's stream
// port answers the node, which delays the match and serves it to viewers who join at different
// times: each one's stream plays to its end at the same state, a viewer's say reaches every viewer,
// its say_game and the viewer list go up to the server; a password asks for SHA3-512.
//
// ge:summary Tests: the classic TV node: the kept match, the late viewer's catch-up, the relay
// ge:keywords test tv-node relay catch-up late-join mv_apply mv_catch_up viewers say_game qul
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/net/classic_mv_state.hpp"
#include "shooters_edge/net/classic_qtv.hpp"
#include "shooters_edge/net/classic_tv.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;

namespace {

std::vector<std::vector<uint8_t>> records_of(const std::vector<uint8_t>& bytes) {
  std::vector<std::vector<uint8_t>> out;
  size_t at = 0;
  while (at < bytes.size()) {
    cn::MvRecord h;
    if (cn::mv_record(std::span<const uint8_t>(bytes).subspan(at), h) != cn::MvSplit::ok) break;
    out.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                     bytes.begin() + static_cast<std::ptrdiff_t>(at + h.whole));
    at += h.whole;
  }
  return out;
}

// The state a reader ends at, as text: players, frags, names, the tracked stats, entities.
std::string end_state(const std::vector<uint8_t>& bytes, std::string& error, int64_t* frames) {
  cl::Demo d;
  if (!cl::open_demo(bytes, d, error)) return "unread";
  for (;;) {
    const cl::DemoRead r = cl::next_frame(d, error);
    if (r == cl::DemoRead::error) return "broke";
    if (r == cl::DemoRead::end) break;
  }
  if (frames) *frames = d.state.frames;
  std::string t;
  const auto f = [](float v) { return std::to_string(static_cast<int>(v * 8)); };
  for (size_t k = 0; k < d.state.players.size(); k++) {
    const cl::DemoEntity& p = d.state.players[k];
    if (!p.present) continue;
    t += "p" + std::to_string(k) + " " + f(p.origin.x) + "," + f(p.origin.y) + "," + f(p.origin.z) +
         " " + std::to_string(d.state.frags[k]) + " " + d.state.names[k] + "\n";
  }
  for (int i = 0; i < 32; i++) t += std::to_string(d.state.stats[i]) + " ";
  t += "\n";
  for (size_t e = 0; e < d.state.entities.size(); e++) {
    const cl::DemoEntity& x = d.state.entities[e];
    if (x.present)
      t += "e" + std::to_string(e) + " " + std::to_string(x.model) + " " + f(x.origin.x) + "," +
           f(x.origin.y) + "," + f(x.origin.z) + "\n";
  }
  return t;
}

}  // namespace

TEST(a_record_waits_for_its_bytes_and_a_damaged_one_is_refused) {
  std::vector<uint8_t> rec;
  const std::vector<uint8_t> body = {8, 3, 'h', 'i', 0};
  cn::mv_put_record(rec, 12, cn::kMvMultiple, 0, 0x5, body);
  cn::MvRecord h;
  CHECK(cn::mv_record(rec, h) == cn::MvSplit::ok);
  CHECK(h.ms == 12 && h.kind == cn::kMvMultiple && h.mask == 0x5 && h.size == 5);
  CHECK(h.whole == rec.size());
  CHECK(cn::mv_record(std::span<const uint8_t>(rec).first(rec.size() - 1), h) == cn::MvSplit::more);
  std::vector<uint8_t> bad = rec;
  bad[1] = 7;  // a kind outside 1..6
  CHECK(cn::mv_record(bad, h) == cn::MvSplit::bad);
  std::vector<uint8_t> huge;
  cn::mv_put_record(huge, 0, cn::kMvAll, 0, 0, std::vector<uint8_t>(cn::kMostRecordBody + 1, 1));
  CHECK(cn::mv_record(huge, h) == cn::MvSplit::bad);
}

TEST(a_late_viewer_caught_up_ends_where_a_whole_viewer_ends) {
  const std::vector<uint8_t> match = cl::write_multiview_match(cl::highlight_match());
  const std::vector<std::vector<uint8_t>> recs = records_of(match);
  CHECK(recs.size() > 100);
  std::string error;
  const std::string whole = end_state(match, error, nullptr);
  CHECK_MSG(whole != "unread" && whole != "broke", error);
  CHECK(whole.find("p0 ") != std::string::npos && whole.find("p3 ") != std::string::npos);
  for (const size_t cut : {size_t{3}, recs.size() / 4, recs.size() / 2, recs.size() - 10}) {
    cn::MvState s;
    for (size_t i = 0; i < cut; i++) cn::mv_apply(s, recs[i]);
    CHECK_MSG(!s.broken, s.why);
    std::vector<uint8_t> joined = cn::mv_catch_up(s);
    CHECK(joined.size() < match.size() / 4);
    for (size_t i = cut; i < recs.size(); i++)
      joined.insert(joined.end(), recs[i].begin(), recs[i].end());
    const std::string late = end_state(joined, error, nullptr);
    std::string why = "joined at record " + std::to_string(cut);
    why += ": " + error;
    why += "\n" + late;
    why += "\nagainst\n" + whole;
    CHECK_MSG(late == whole, why);
  }
}

TEST(viewer_names_are_cleaned_and_kept_apart) {
  ge::core::Rng rng{3};
  CHECK(cn::tv_viewer_name("  eve ", {}, rng) == "eve");
  CHECK(cn::tv_viewer_name("", {}, rng) == "unnamed");
  CHECK(cn::tv_viewer_name("console", {}, rng) == "unnamed");
  CHECK(cn::tv_viewer_name("a#b", {}, rng) == "unnamed");
  CHECK(cn::tv_viewer_name("x:y", {}, rng) == "unnamed");
  const std::string twin = cn::tv_viewer_name("eve", {"eve"}, rng);
  CHECK(twin.size() == 13 && twin.starts_with("eve(") && twin.back() == ')');
  const std::string longer =
      cn::tv_viewer_name("a very long viewer name", {"a very long viewer name"}, rng);
  CHECK(longer.size() == 25 && longer.starts_with("a very long viewer name("));
}

namespace {

// A made-up server's stream port: answers the first request with BEGIN and then hands the match's
// records out a frame at a time.
struct Upstream {
  gn::TcpListener listener;
  gn::TcpStream tcp;
  bool begun = false;
  std::string request;
  std::vector<uint8_t> up;
  std::vector<std::string> commands;
  size_t next = 0;
};

struct Viewer {
  gn::TcpStream tcp;
  std::vector<uint8_t> got;
  double join_s = 0;
  bool dialed = false;
};

void pump_upstream(Upstream& u, const std::vector<std::vector<uint8_t>>& recs, bool frame) {
  if (u.tcp.handle < 0) gn::tcp_accept(u.listener, u.tcp);
  if (u.tcp.handle < 0) return;
  gn::tcp_pump(u.tcp);
  std::vector<uint8_t> got;
  gn::tcp_take_bytes(u.tcp, got);
  if (!u.begun) {
    u.request.append(got.begin(), got.end());
    cn::QtvRequest q;
    size_t used = 0;
    if (cn::qtv_parse_request(u.request, q, used) != cn::QtvParse::ok) return;
    u.begun = true;
    cn::QtvAnswer a;
    const std::string text = cn::qtv_answer_text(a);
    gn::tcp_send_bytes(u.tcp, reinterpret_cast<const uint8_t*>(text.data()), text.size());
    return;
  }
  u.up.insert(u.up.end(), got.begin(), got.end());
  size_t at = 0;
  for (;;) {
    std::string cmd;
    size_t used = 0;
    if (cn::qtv_next_up(u.up.data() + at, u.up.size() - at, cmd, used) != cn::QtvParse::ok) break;
    u.commands.push_back(cmd);
    at += used;
  }
  u.up.erase(u.up.begin(), u.up.begin() + static_cast<std::ptrdiff_t>(at));
  if (!frame) return;
  // The records up to and through the next frame's first.
  std::vector<uint8_t> out;
  bool framed = false;
  while (u.next < recs.size()) {
    const std::vector<uint8_t>& r = recs[u.next];
    if (r[0] != 0 && framed) break;
    framed = framed || r[0] != 0;
    out.insert(out.end(), r.begin(), r.end());
    u.next++;
  }
  if (!out.empty()) gn::tcp_send_bytes(u.tcp, out.data(), out.size());
}

void dial(Viewer& v, const gn::Address& to, const cn::QtvRequest& q) {
  std::string error;
  v.dialed = gn::tcp_connect(v.tcp, to, &error);
  for (int i = 0; i < 100000 && v.tcp.connecting; i++) gn::tcp_pump(v.tcp);
  const std::string text = cn::qtv_request_text(q);
  gn::tcp_send_bytes(v.tcp, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

}  // namespace

TEST(the_node_relays_a_match_to_viewers_who_join_late) {
  const std::vector<uint8_t> match = cl::write_multiview_match(cl::highlight_match());
  const std::vector<std::vector<uint8_t>> recs = records_of(match);
  std::string error;
  const std::string whole = end_state(match, error, nullptr);
  Upstream u;
  CHECK(gn::tcp_listen(u.listener, gn::loopback_v4(0), &error));
  cn::TvNode n;
  cn::TvConfig cfg;
  cfg.delay_s = 0.5;
  CHECK_MSG(cn::tv_open(n, gn::loopback_v4(0), u.listener.local, cfg, error), error);
  std::vector<Viewer> viewers(3);
  viewers[0].join_s = 0;
  viewers[1].join_s = 8;
  viewers[2].join_s = 20;
  const double step = 0.05;  // the match's frame
  bool said = false;
  for (int t = 0; t < 2000; t++) {
    const double now = t * step;
    pump_upstream(u, recs, true);
    for (size_t i = 0; i < viewers.size(); i++) {
      Viewer& v = viewers[i];
      if (!v.dialed && now >= v.join_s) {
        cn::QtvRequest q;
        q.version = 1;
        q.receive = true;
        q.userinfo = "\\name\\eve";
        dial(v, n.listener.local, q);
      }
    }
    cn::tv_update(n, now);
    for (Viewer& v : viewers)
      if (v.dialed) {
        gn::tcp_pump(v.tcp);
        gn::tcp_take_bytes(v.tcp, v.got);
      }
    if (!said && now >= 25) {
      said = true;
      std::vector<uint8_t> b;
      cn::qtv_put_up(b, "say \"hello all\"");
      cn::qtv_put_up(b, "say_game \"good game\"");
      gn::tcp_send_bytes(viewers[0].tcp, b.data(), b.size());
    }
    if (u.next >= recs.size() && n.pending.empty() && now > 40) break;
  }
  for (int i = 0; i < 50; i++) {
    cn::tv_update(n, 1000);
    for (Viewer& v : viewers) {
      gn::tcp_pump(v.tcp);
      gn::tcp_take_bytes(v.tcp, v.got);
    }
    pump_upstream(u, recs, false);
  }
  CHECK(n.counts.viewers == 3 && n.counts.levels == 1);
  CHECK(n.counts.released == static_cast<int64_t>(recs.size()));
  for (size_t i = 0; i < viewers.size(); i++) {
    const std::string text(viewers[i].got.begin(), viewers[i].got.end());
    cn::QtvAnswer a;
    size_t used = 0;
    CHECK(cn::qtv_parse_answer(text, a, used) == cn::QtvParse::ok &&
          a.kind == cn::QtvAnswerKind::begin);
    const std::vector<uint8_t> stream(viewers[i].got.begin() + static_cast<std::ptrdiff_t>(used),
                                      viewers[i].got.end());
    const std::string end = end_state(stream, error, nullptr);
    CHECK_MSG(end == whole, "viewer " + std::to_string(i + 1) + ": " + error);
    CHECK(text.find("#0:qtv_say:#1:eve: hello all") != std::string::npos);
  }
  // The second and third are kept apart from the first's name.
  bool game = false, joined = false;
  for (const std::string& c : u.commands) {
    game = game || c == "say_game \"#1:eve: good game\"";
    joined = joined || c == "qul 1 1 \"eve\"";
  }
  CHECK(game && joined);
  CHECK(n.counts.game_chat == 1 && n.counts.chat == 1);
  cn::tv_close(n);
  gn::tcp_close(u.tcp);
  gn::tcp_close_listener(u.listener);
}

TEST(a_node_with_a_password_asks_for_sha3_and_refuses_a_wrong_one) {
  std::string error;
  gn::TcpListener nowhere;
  CHECK(gn::tcp_listen(nowhere, gn::loopback_v4(0), &error));
  cn::TvNode n;
  cn::TvConfig cfg;
  cfg.password = "secret";
  CHECK(cn::tv_open(n, gn::loopback_v4(0), nowhere.local, cfg, error));
  const auto talk = [&](Viewer& v, const cn::QtvRequest& q) {
    dial(v, n.listener.local, q);
    for (int i = 0; i < 400; i++) {
      cn::tv_update(n, 0.01 * i);
      gn::tcp_pump(v.tcp);
      gn::tcp_take_bytes(v.tcp, v.got);
      cn::QtvAnswer a;
      size_t used = 0;
      if (cn::qtv_parse_answer(std::string(v.got.begin(), v.got.end()), a, used) ==
          cn::QtvParse::ok)
        return a;
    }
    return cn::QtvAnswer{cn::QtvAnswerKind::terror, "no answer", {}, {}};
  };
  cn::QtvRequest q;
  q.version = 1;
  q.receive = true;
  q.offers = {cn::QtvAuth::plain, cn::QtvAuth::sha3_512};
  Viewer good, bad;
  const cn::QtvAnswer ask = talk(good, q);
  CHECK(ask.kind == cn::QtvAnswerKind::auth && ask.method == cn::QtvAuth::sha3_512);
  CHECK(ask.challenge.size() == 63);
  cn::QtvRequest answered = q;
  answered.has_password = true;
  answered.password = cn::qtv_auth_response(ask.method, ask.challenge, "secret");
  good.got.clear();
  const std::string text = cn::qtv_request_text(answered);
  gn::tcp_send_bytes(good.tcp, reinterpret_cast<const uint8_t*>(text.data()), text.size());
  cn::QtvAnswer let;
  for (int i = 0; i < 400; i++) {
    cn::tv_update(n, 5 + 0.01 * i);
    gn::tcp_pump(good.tcp);
    gn::tcp_take_bytes(good.tcp, good.got);
    size_t used = 0;
    if (cn::qtv_parse_answer(std::string(good.got.begin(), good.got.end()), let, used) ==
        cn::QtvParse::ok)
      break;
  }
  CHECK(let.kind == cn::QtvAnswerKind::begin);
  cn::QtvRequest wrong = q;
  wrong.offers = {cn::QtvAuth::plain};
  wrong.has_password = true;
  wrong.password = "guess";
  const cn::QtvAnswer no = talk(bad, wrong);
  CHECK(no.kind == cn::QtvAnswerKind::perror && no.text == "Bad password.");
  cn::tv_close(n);
  gn::tcp_close_listener(nowhere);
}

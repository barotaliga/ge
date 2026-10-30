// The simulated Discord client (sim_discord.hpp): the client's side of each frame, its limits,
// and the journal.
//
// ge:summary The simulated Discord client's frames, limits, asks, joins and journal
// ge:keywords services discord sim client ready subscribe set-activity error close journal
// ge:skill services
// ge:test tests/services_discord_test.cpp
#include "services/sim_discord.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>

#include "core/json_read.hpp"

namespace ge::services {

struct SimDiscord::State {
  UserId player = 0;
  std::string name;
  bool running = true;
  bool linked = false;
  bool greeted = false;
  bool dead_after = false;  // a close was sent: the link dies once it is delivered
  uint64_t generation = 0;  // moves when a link is cut, so an old link finds itself gone
  std::string rx, tx;
  bool sub_join = false, sub_request = false;
  std::string shown;
  bool shown_join = false;
  std::deque<int64_t> sets;
  std::vector<UserId> asked;  // asks not answered yet
  std::vector<std::pair<UserId, bool>> answers;
  int updates = 0, refusals = 0, pongs = 0, pings = 0;
  int64_t now_us = 0;
  std::vector<std::string> journal;

  void log(const std::string& line) {
    char t[24];
    std::snprintf(t, sizeof t, "%.3f ", static_cast<double>(now_us) / 1e6);
    journal.push_back(t + line);
  }
  void send(DiscordOp op, const std::string& json) { tx += discord_frame(op, json); }
  void reply(const std::string& cmd, const std::string& data, const std::string& nonce) {
    send(DiscordOp::frame, "{\"cmd\":\"" + cmd + "\",\"data\":" + data +
                               ",\"evt\":null,\"nonce\":" + json_quote(nonce) + "}");
  }
  void error(const std::string& cmd, int code, const std::string& message,
             const std::string& nonce) {
    refusals++;
    log("app> ERROR " + cmd + " " + std::to_string(code) + " " + message);
    send(DiscordOp::frame, "{\"cmd\":\"" + cmd + "\",\"data\":{\"code\":" + std::to_string(code) +
                               ",\"message\":" + json_quote(message) +
                               "},\"evt\":\"ERROR\",\"nonce\":" + json_quote(nonce) + "}");
  }
  void close(int code, const std::string& message) {
    refusals++;
    log("app> CLOSE " + std::to_string(code) + " " + message);
    send(DiscordOp::close,
         "{\"code\":" + std::to_string(code) + ",\"message\":" + json_quote(message) + "}");
    dead_after = true;
  }
  void cut() {
    linked = greeted = dead_after = false;
    sub_join = sub_request = false;
    shown.clear();
    shown_join = false;
    asked.clear();
    rx.clear();
    tx.clear();
    generation++;
  }

  void handle(DiscordOp op, const std::string& json) {
    core::FlatJson m;
    std::string why;
    if (!json.empty() && !core::read_flat(json, m, why, true)) {
      close(1003, "unreadable JSON: " + why);
      return;
    }
    const auto text = [&](const std::string& key) {
      const auto it = m.find(key);
      if (it == m.end()) return std::string();
      return it->second.kind == core::JsonValue::Kind::string ? it->second.text
                                                              : core::json_text(it->second);
    };
    if (op == DiscordOp::handshake) {
      const std::string id = text("client_id");
      bool digits = !id.empty();
      for (const char c : id) digits = digits && c >= '0' && c <= '9';
      if (text("v") != "1" || !digits) {
        close(4000, "Invalid Client ID");
        return;
      }
      greeted = true;
      log("game> HANDSHAKE " + id);
      log("app> READY " + name);
      const std::string uid = std::to_string(player);
      send(DiscordOp::frame,
           "{\"cmd\":\"DISPATCH\",\"data\":{\"v\":1,\"config\":{\"environment\":\"production\"},"
           "\"user\":{\"id\":\"" +
               uid + "\",\"username\":" + json_quote(name) +
               ",\"discriminator\":\"0\",\"global_name\":" + json_quote(name) +
               ",\"avatar\":null}},\"evt\":\"READY\",\"nonce\":null}");
      return;
    }
    if (op == DiscordOp::pong) {
      pongs++;
      log("game> PONG");
      return;
    }
    if (op == DiscordOp::ping) {
      send(DiscordOp::pong, json);
      return;
    }
    if (op == DiscordOp::close) {
      log("game> CLOSE");
      cut();
      return;
    }
    if (!greeted) {
      close(1003, "a command before the handshake");
      return;
    }
    const std::string cmd = text("cmd");
    const std::string nonce = text("nonce");
    if (cmd == "SUBSCRIBE") {
      const std::string evt = text("evt");
      if (evt == "ACTIVITY_JOIN") {
        sub_join = true;
      } else if (evt == "ACTIVITY_JOIN_REQUEST") {
        sub_request = true;
      } else {
        error(cmd, 4000, "no such event " + evt, nonce);
        return;
      }
      log("game> SUBSCRIBE " + evt);
      reply(cmd, "{\"evt\":\"" + evt + "\"}", nonce);
    } else if (cmd == "SET_ACTIVITY") {
      if (std::strtoll(text("args.pid").c_str(), nullptr, 10) <= 0) {
        error(cmd, 4000, "args.pid is missing", nonce);
        return;
      }
      while (!sets.empty() && now_us - sets.front() >= kDiscordUpdateWindowUs) sets.pop_front();
      if (static_cast<int>(sets.size()) >= kDiscordUpdates) {
        error(cmd, 5000, "rate limited: 5 activity updates in 20 s", nonce);
        return;
      }
      const auto act = m.find("args.activity");
      if (act != m.end() && act->second.kind == core::JsonValue::Kind::null) {
        sets.push_back(now_us);
        updates++;
        shown.clear();
        shown_join = false;
        log("game> SET_ACTIVITY (cleared)");
        reply(cmd, "null", nonce);
        return;
      }
      Activity a;
      const std::string p = "args.activity.";
      a.details = text(p + "details");
      a.state = text(p + "state");
      a.start_ms = std::strtoll(text(p + "timestamps.start").c_str(), nullptr, 10);
      a.end_ms = std::strtoll(text(p + "timestamps.end").c_str(), nullptr, 10);
      a.large_image = text(p + "assets.large_image");
      a.large_text = text(p + "assets.large_text");
      a.small_image = text(p + "assets.small_image");
      a.small_text = text(p + "assets.small_text");
      a.party_id = text(p + "party.id");
      a.party_size = std::atoi(text(p + "party.size.0").c_str());
      a.party_max = std::atoi(text(p + "party.size.1").c_str());
      a.join_secret = text(p + "secrets.join");
      if (const std::string bad = check_activity(a); !bad.empty()) {
        error(cmd, 4000, bad, nonce);
        return;
      }
      sets.push_back(now_us);
      updates++;
      shown = activity_json(a);
      shown_join = !a.join_secret.empty();
      log("game> SET_ACTIVITY " + shown);
      reply(cmd, shown, nonce);
    } else if (cmd == "SEND_ACTIVITY_JOIN_INVITE" || cmd == "CLOSE_ACTIVITY_REQUEST") {
      const UserId who = std::strtoull(text("args.user_id").c_str(), nullptr, 10);
      size_t i = 0;
      while (i < asked.size() && asked[i] != who) i++;
      if (i == asked.size()) {
        error(cmd, 4000, "no ask to join from " + std::to_string(who), nonce);
        return;
      }
      asked.erase(asked.begin() + static_cast<std::ptrdiff_t>(i));
      const bool accept = cmd == "SEND_ACTIVITY_JOIN_INVITE";
      answers.emplace_back(who, accept);
      log("game> " + cmd + " " + std::to_string(who));
      reply(cmd, "null", nonce);
    } else {
      error(cmd, 4000, "unknown command", nonce);
    }
  }
};

namespace {

class SimLink final : public DiscordLink {
 public:
  explicit SimLink(std::shared_ptr<SimDiscord::State> s) : s_(std::move(s)) {}
  ~SimLink() override {
    // A destructor must not throw: a close whose journal line cannot be written (memory) is said
    // on stderr instead.
    try {
      close();
    } catch (const std::exception& e) {
      std::fprintf(stderr,
                   "discord: the simulated link's close threw in its destructor (%s); the "
                   "journal lacks its last line.\n",
                   e.what());
    }
  }

  bool open(std::string* error) override {
    if (!s_->running) {
      if (error) *error = "discord: the simulated app is not running";
      return false;
    }
    s_->cut();  // a link opened again replaces the last
    s_->linked = true;
    gen_ = s_->generation;
    s_->log("link opened");
    return true;
  }

  bool exchange(int64_t now_us, const std::string& out, std::string& in) override {
    SimDiscord::State& s = *s_;
    if (!mine()) return false;
    if (s.dead_after) {
      s.log("link closed by the app");
      s.cut();
      return false;
    }
    s.now_us = now_us;
    s.rx += out;
    DiscordOp op{};
    std::string json;
    while (mine() && !s.dead_after) {
      const FrameRead r = discord_next_frame(s.rx, op, json);
      if (r == FrameRead::none) break;
      if (r == FrameRead::bad) {
        s.close(1003, "a bad frame");
        break;
      }
      s.handle(op, json);
    }
    if (!mine()) return false;
    in += s.tx;
    s.tx.clear();
    return true;
  }

  void close() override {
    if (mine()) {
      s_->log("link closed by the game");
      s_->cut();
    }
  }

  int64_t process_id() const override { return 4242; }

 private:
  bool mine() const { return s_->linked && s_->generation == gen_; }
  std::shared_ptr<SimDiscord::State> s_;
  uint64_t gen_ = 0;
};

}  // namespace

SimDiscord::SimDiscord(UserId player, std::string name) : state_(std::make_shared<State>()) {
  state_->player = player;
  state_->name = std::move(name);
}

SimDiscord::~SimDiscord() = default;

std::unique_ptr<DiscordLink> SimDiscord::link() { return std::make_unique<SimLink>(state_); }

void SimDiscord::set_running(bool running) {
  State& s = *state_;
  if (s.running == running) return;
  s.running = running;
  s.log(running ? "app started" : "app quit");
  if (!running) s.cut();
}

void SimDiscord::tick(int64_t now_us) { state_->now_us = now_us; }

bool SimDiscord::running() const { return state_->running; }

void SimDiscord::ask_to_join(UserId friend_id, const std::string& name) {
  State& s = *state_;
  if (!s.greeted || !s.sub_request || !s.shown_join) {
    s.log("friend " + name + " cannot ask to join (no join secret shown, or no subscription)");
    return;
  }
  s.asked.push_back(friend_id);
  s.log("app> ACTIVITY_JOIN_REQUEST " + name);
  s.send(DiscordOp::frame, "{\"cmd\":\"DISPATCH\",\"data\":{\"user\":{\"id\":\"" +
                               std::to_string(friend_id) + "\",\"username\":" + json_quote(name) +
                               ",\"discriminator\":\"0\",\"avatar\":null}},"
                               "\"evt\":\"ACTIVITY_JOIN_REQUEST\",\"nonce\":null}");
}

void SimDiscord::join(const std::string& secret) {
  State& s = *state_;
  if (!s.greeted || !s.sub_join) {
    s.log("join pressed with no game subscribed");
    return;
  }
  s.log("app> ACTIVITY_JOIN " + secret);
  s.send(DiscordOp::frame, "{\"cmd\":\"DISPATCH\",\"data\":{\"secret\":" + json_quote(secret) +
                               "},\"evt\":\"ACTIVITY_JOIN\",\"nonce\":null}");
}

void SimDiscord::ping() {
  State& s = *state_;
  if (!s.greeted) return;
  s.log("app> PING");
  s.send(DiscordOp::ping, "{\"nonce\":\"p" + std::to_string(++s.pings) + "\"}");
}

std::string SimDiscord::shown() const { return state_->shown; }
std::vector<std::pair<UserId, bool>> SimDiscord::answers() const { return state_->answers; }
int SimDiscord::updates() const { return state_->updates; }
int SimDiscord::refusals() const { return state_->refusals; }
int SimDiscord::pongs() const { return state_->pongs; }
const std::vector<std::string>& SimDiscord::journal() const { return state_->journal; }

}  // namespace ge::services

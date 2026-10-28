// Discord's wire (frames, the activity as JSON, its limits) and the client: finding Discord,
// the handshake, the subscriptions, the latest activity sent under the rate limit, and the
// client's messages turned into events.
//
// ge:summary Discord's frames and activity JSON, and the presence client's state machine
// ge:keywords services discord frame activity json handshake ready subscribe set-activity
// ge:keywords activity-join activity-join-request send-activity-join-invite close-activity-request
// ge:keywords rate-limit coalesce backoff
// ge:skill services
// ge:test tests/services_discord_test.cpp
#include "services/discord.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "core/json_read.hpp"

namespace ge::services {

namespace {

std::string text_limit(const char* what, const std::string& s, size_t max) {
  if (s.empty() || (s.size() >= kActivityTextMin && s.size() <= max)) return {};
  return std::string("activity: ")
      .append(what)
      .append(" is ")
      .append(std::to_string(s.size()))
      .append(" bytes; Discord takes ")
      .append(std::to_string(kActivityTextMin))
      .append(" to ")
      .append(std::to_string(max))
      .append("; next: shorten it, or leave it empty");
}

void put_u32(std::string& out, uint32_t v) {
  for (int i = 0; i < 4; i++) out.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
}

uint32_t get_u32(const std::string& s, size_t at) {
  uint32_t v = 0;
  for (int i = 0; i < 4; i++)
    v |= static_cast<uint32_t>(static_cast<uint8_t>(s[at + static_cast<size_t>(i)])) << (8 * i);
  return v;
}

std::string text_of(const core::FlatJson& m, const std::string& key) {
  const auto it = m.find(key);
  if (it == m.end()) return {};
  return it->second.kind == core::JsonValue::Kind::string ? it->second.text
                                                          : core::json_text(it->second);
}

}  // namespace

std::string check_activity(const Activity& a) {
  for (const std::string& f : {text_limit("details", a.details, kActivityTextMax),
                               text_limit("state", a.state, kActivityTextMax),
                               text_limit("large_text", a.large_text, kActivityTextMax),
                               text_limit("small_text", a.small_text, kActivityTextMax),
                               text_limit("party_id", a.party_id, kActivityTextMax),
                               text_limit("join_secret", a.join_secret, kActivityTextMax)})
    if (!f.empty()) return f;
  if (a.large_image.size() > kActivityImageMax || a.small_image.size() > kActivityImageMax)
    return "activity: an image key is over " + std::to_string(kActivityImageMax) +
           " bytes; next: use the asset's key from the developer portal";
  if (a.party_size != 0 || a.party_max != 0) {
    if (a.party_size < 1 || a.party_max < a.party_size)
      return "activity: party " + std::to_string(a.party_size) + " of " +
             std::to_string(a.party_max) +
             " is not 1 <= size <= max; next: set both, or both to 0 for no party";
  }
  if (!a.join_secret.empty() && (a.party_id.empty() || a.party_max == 0))
    return "activity: a join secret needs a party (party_id and party_size of party_max); "
           "next: set the party the joiner joins";
  if (a.start_ms < 0 || a.end_ms < 0 || (a.end_ms != 0 && a.end_ms < a.start_ms))
    return "activity: the timestamps are negative or end before they start; next: pass unix "
           "time in ms, or 0";
  return {};
}

std::string json_quote(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
      out.push_back(c);
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
      out += buf;
    } else {
      out.push_back(c);
    }
  }
  out.push_back('"');
  return out;
}

std::string activity_json(const Activity& a) {
  std::string o = "{";
  const auto field = [&](const char* key, const std::string& value) {
    if (value.empty()) return;
    if (o.size() > 1) o += ",";
    o.append("\"").append(key).append("\":").append(value);
  };
  field("details", a.details.empty() ? "" : json_quote(a.details));
  field("state", a.state.empty() ? "" : json_quote(a.state));
  if (a.start_ms || a.end_ms) {
    std::string t = "{";
    if (a.start_ms) t += "\"start\":" + std::to_string(a.start_ms);
    if (a.end_ms)
      t.append(a.start_ms ? "," : "").append("\"end\":").append(std::to_string(a.end_ms));
    field("timestamps", t + "}");
  }
  std::string assets;
  for (const auto& [key, value] :
       {std::pair<const char*, const std::string*>{"large_image", &a.large_image},
        {"large_text", &a.large_text},
        {"small_image", &a.small_image},
        {"small_text", &a.small_text}})
    if (!value->empty())
      assets.append(assets.empty() ? "" : ",")
          .append("\"")
          .append(key)
          .append("\":")
          .append(json_quote(*value));
  if (!assets.empty()) field("assets", "{" + assets + "}");
  if (!a.party_id.empty() || a.party_max) {
    std::string p = "{";
    if (!a.party_id.empty()) p += "\"id\":" + json_quote(a.party_id);
    if (a.party_max)
      p.append(a.party_id.empty() ? "" : ",")
          .append("\"size\":[")
          .append(std::to_string(a.party_size))
          .append(",")
          .append(std::to_string(a.party_max))
          .append("]");
    field("party", p + "}");
  }
  if (!a.join_secret.empty()) field("secrets", "{\"join\":" + json_quote(a.join_secret) + "}");
  field("instance", "false");
  return o + "}";
}

std::string discord_frame(DiscordOp op, std::string_view json) {
  std::string out;
  out.reserve(8 + json.size());
  put_u32(out, static_cast<uint32_t>(op));
  put_u32(out, static_cast<uint32_t>(json.size()));
  out.append(json);
  return out;
}

FrameRead discord_next_frame(std::string& buffer, DiscordOp& op, std::string& json) {
  if (buffer.size() < 8) return FrameRead::none;
  const uint32_t code = get_u32(buffer, 0);
  const uint32_t len = get_u32(buffer, 4);
  if (code > static_cast<uint32_t>(DiscordOp::pong) || len > kDiscordMaxFrame)
    return FrameRead::bad;
  if (buffer.size() < 8 + size_t{len}) return FrameRead::none;
  op = static_cast<DiscordOp>(code);
  json.assign(buffer, 8, len);
  buffer.erase(0, 8 + size_t{len});
  return FrameRead::whole;
}

// ---- the client

struct Discord::Impl {
  enum class State : uint8_t { idle, handshaking, ready };
  DiscordConfig config;
  std::unique_ptr<DiscordLink> link;
  State state = State::idle;
  int64_t next_try_us = 0;
  int64_t backoff_us = 0;
  std::string out, in;
  UserInfo user;
  Activity wanted;
  bool has_wanted = false;
  bool dirty = false;         // `wanted` differs from what the client was last sent
  int64_t last_send_us = -1;  // when the last update went, for the rate limit
  uint64_t nonce = 0;
  DiscordCounts counts;

  void command(const std::string& cmd, const std::string& args, const std::string& evt = {}) {
    std::string j = "{\"cmd\":\"" + cmd + "\",\"args\":" + args;
    if (!evt.empty()) j += ",\"evt\":\"" + evt + "\"";
    j += ",\"nonce\":\"" + std::to_string(++nonce) + "\"}";
    out += discord_frame(DiscordOp::frame, j);
  }

  void lose(int64_t now_us, std::vector<Event>& events) {
    if (state == State::ready) {
      Event e;
      e.kind = EventKind::connection_lost;
      e.id = "discord";
      events.push_back(std::move(e));
    }
    link->close();
    state = State::idle;
    out.clear();
    in.clear();
    last_send_us = -1;  // a new link starts a new limit
    backoff_us = config.retry_us;
    next_try_us = now_us + backoff_us;
  }

  void read(int64_t now_us, std::vector<Event>& events) {
    DiscordOp op{};
    std::string json;
    while (state != State::idle) {
      const FrameRead r = discord_next_frame(in, op, json);
      if (r == FrameRead::none) return;
      if (r == FrameRead::bad) {
        refused(events, "frame", "a frame with an unknown op or over 64 KiB; the link was closed");
        lose(now_us, events);
        return;
      }
      core::FlatJson m;
      std::string why;
      if (!json.empty() && !core::read_flat(json, m, why, true)) {
        refused(events, "frame", "unreadable JSON from the client: " + why);
        continue;
      }
      if (op == DiscordOp::ping) {
        out += discord_frame(DiscordOp::pong, json);
      } else if (op == DiscordOp::close) {
        refused(events, state == State::handshaking ? "handshake" : "link",
                "the client closed the link: " + text_of(m, "message") + " (code " +
                    text_of(m, "code") + "); next: check DiscordConfig::client_id");
        lose(now_us, events);
        return;
      } else if (op == DiscordOp::frame) {
        dispatch(m, events);
      }
    }
  }

  void refused(std::vector<Event>& events, std::string id, std::string why) {
    Event e;
    e.kind = EventKind::call_refused;
    e.id = std::move(id);
    e.text = std::move(why);
    events.push_back(std::move(e));
  }

  void dispatch(const core::FlatJson& m, std::vector<Event>& events) {
    const std::string cmd = text_of(m, "cmd");
    const std::string evt = text_of(m, "evt");
    if (evt == "ERROR") {
      refused(events, cmd, text_of(m, "data.message") + " (code " + text_of(m, "data.code") + ")");
      return;
    }
    if (cmd != "DISPATCH") return;  // a command's plain answer
    if (evt == "READY") {
      state = State::ready;
      counts.connects++;
      backoff_us = config.retry_us;
      user.id = std::strtoull(text_of(m, "data.user.id").c_str(), nullptr, 10);
      user.name = text_of(m, "data.user.global_name");
      if (user.name.empty() || user.name == "null") user.name = text_of(m, "data.user.username");
      command("SUBSCRIBE", "{}", "ACTIVITY_JOIN");
      command("SUBSCRIBE", "{}", "ACTIVITY_JOIN_REQUEST");
      dirty = has_wanted;  // a new link shows nothing until it is sent again
      Event e;
      e.kind = EventKind::connection_restored;
      e.id = "discord";
      e.user = user.id;
      e.text = user.name;
      events.push_back(std::move(e));
    } else if (evt == "ACTIVITY_JOIN") {
      Event e;
      e.kind = EventKind::join_requested;
      e.text = text_of(m, "data.secret");
      events.push_back(std::move(e));
    } else if (evt == "ACTIVITY_JOIN_REQUEST") {
      Event e;
      e.kind = EventKind::join_asked;
      e.user = std::strtoull(text_of(m, "data.user.id").c_str(), nullptr, 10);
      e.text = text_of(m, "data.user.username");
      events.push_back(std::move(e));
    }
  }

  void send_activity(int64_t now_us) {
    if (state != State::ready || !dirty) return;
    // Paced, not burst: one update every window / updates, so a game that changes its line
    // constantly shows a line at most that old, and no window ever holds more than the limit.
    if (last_send_us >= 0 && now_us - last_send_us < kDiscordUpdateGapUs) {
      counts.held++;
      return;
    }
    const std::string activity = has_wanted ? activity_json(wanted) : "null";
    command("SET_ACTIVITY",
            "{\"pid\":" + std::to_string(link->process_id()) + ",\"activity\":" + activity + "}");
    last_send_us = now_us;
    counts.sent++;
    dirty = false;
  }
};

Discord::Discord(DiscordConfig config, std::unique_ptr<DiscordLink> link)
    : impl_(std::make_unique<Impl>()) {
  impl_->config = std::move(config);
  impl_->link = std::move(link);
  impl_->backoff_us = impl_->config.retry_us;
}

Discord::~Discord() {
  if (impl_->link) impl_->link->close();
}

void Discord::poll(int64_t now_us, std::vector<Event>& out) {
  Impl& s = *impl_;
  if (s.state == Impl::State::idle) {
    if (now_us < s.next_try_us) return;
    s.counts.attempts++;
    if (!s.link->open(nullptr)) {
      s.next_try_us = now_us + s.backoff_us;
      s.backoff_us = std::min(s.backoff_us * 2, s.config.retry_max_us);
      return;
    }
    s.state = Impl::State::handshaking;
    s.out = discord_frame(DiscordOp::handshake,
                          "{\"v\":1,\"client_id\":" + json_quote(s.config.client_id) + "}");
  }
  s.send_activity(now_us);
  const std::string sending = std::move(s.out);
  s.out.clear();
  if (!s.link->exchange(now_us, sending, s.in)) {
    s.lose(now_us, out);
    return;
  }
  s.read(now_us, out);
}

Result Discord::set_activity(const Activity& a) {
  if (!check_activity(a).empty()) return Result::invalid;
  Impl& s = *impl_;
  if (s.has_wanted && s.wanted == a) return Result::ok;
  if (s.dirty && s.state == Impl::State::ready) s.counts.coalesced++;
  s.wanted = a;
  s.has_wanted = true;
  s.dirty = true;
  return Result::ok;
}

void Discord::clear_activity() {
  Impl& s = *impl_;
  if (!s.has_wanted) return;
  if (s.dirty && s.state == Impl::State::ready) s.counts.coalesced++;
  s.has_wanted = false;
  s.dirty = true;
}

Result Discord::answer_join(UserId user, bool accept) {
  Impl& s = *impl_;
  if (s.state != Impl::State::ready) return Result::offline;
  s.command(accept ? "SEND_ACTIVITY_JOIN_INVITE" : "CLOSE_ACTIVITY_REQUEST",
            "{\"user_id\":\"" + std::to_string(user) + "\"}");
  return Result::ok;
}

bool Discord::connected() const { return impl_->state == Impl::State::ready; }
UserInfo Discord::user() const { return impl_->user; }
DiscordCounts Discord::counts() const { return impl_->counts; }

}  // namespace ge::services

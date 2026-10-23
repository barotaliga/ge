// The handheld and the extras on the simulated store: the on-screen keyboard (open only where the
// player has no keyboard, closed by the player typing or dismissing it), the machine sleeping and
// waking (a sleeping player's server calls answer `offline`), the recording timeline's marks and
// mode, and remote play (a friend invited, joining and leaving the host's game). The store holds
// each call to the limits a store states for it.
//
// ge:summary The simulated store's extras: on-screen keyboard, sleep and wake, timeline, remote
// play ge:keywords services sim store on-screen-keyboard text_entered suspend resume sleep timeline
// ge:keywords remote-play handheld big-picture
// ge:skill services
// ge:test tests/services_extras_test.cpp
#include <algorithm>
#include <string>

#include "services/sim_store_state.hpp"

namespace ge::services {

namespace {

constexpr size_t kPromptMax = 256;
constexpr uint32_t kKeyboardMax = 1024;
constexpr size_t kMarkTitleMax = 128;
constexpr uint32_t kMarkPriorityMax = 1000;
constexpr double kMarkSpanMax = 600;  // seconds back, and seconds long

const char* mode_name(TimelineMode m) {
  switch (m) {
    case TimelineMode::menus:
      return "menus";
    case TimelineMode::loading:
      return "loading";
    case TimelineMode::playing:
      return "playing";
    case TimelineMode::staging:
      return "staging";
  }
  return "?";
}

bool is_friend(const SimPlayer& p, UserId user) {
  return std::find(p.friends.begin(), p.friends.end(), user) != p.friends.end();
}

}  // namespace

Result extras_keyboard(SimStore::Impl& s, SimPlayer& p, KeyboardMode mode, std::string_view prompt,
                       std::string_view initial, uint32_t max_chars) {
  (void)mode;
  const std::string what = "'" + std::string(prompt) + "' max " + std::to_string(max_chars);
  if (!p.environment.handheld && !p.environment.big_picture)
    return s.log(p, "show_keyboard", what, Result::unsupported);
  if (p.keyboard_max != 0 || prompt.size() > kPromptMax || max_chars == 0 ||
      max_chars > kKeyboardMax || initial.size() > max_chars)
    return s.log(p, "show_keyboard", what, Result::invalid);
  p.keyboard_max = max_chars;
  return s.log(p, "show_keyboard", what, Result::ok);
}

Result extras_timeline_mark(SimStore::Impl& s, SimPlayer& p, const TimelineMark& mark) {
  const bool ok = !mark.icon.empty() && !mark.title.empty() && mark.title.size() <= kMarkTitleMax &&
                  mark.priority <= kMarkPriorityMax && mark.start_offset_s >= -kMarkSpanMax &&
                  mark.start_offset_s <= kMarkSpanMax && mark.duration_s >= 0 &&
                  mark.duration_s <= kMarkSpanMax;
  if (ok) p.timeline.push_back(mark);
  return s.log(p, "timeline_mark", "'" + mark.title + "'", ok ? Result::ok : Result::invalid);
}

Result extras_timeline_mode(SimStore::Impl& s, SimPlayer& p, TimelineMode mode) {
  p.mode = mode;
  return s.log(p, "timeline_mode", mode_name(mode), Result::ok);
}

Result extras_invite_remote(SimStore::Impl& s, SimPlayer& p, UserId user) {
  const SimPlayer* f = s.find(user);
  const std::string what = f == nullptr ? std::to_string(user) : f->name;
  if (f == nullptr || !is_friend(p, user))
    return s.log(p, "invite_remote_play", what, Result::not_found);
  if (p.asleep || !p.online) return s.log(p, "invite_remote_play", what, Result::offline);
  if (std::find(p.remote_invited.begin(), p.remote_invited.end(), user) == p.remote_invited.end())
    p.remote_invited.push_back(user);
  return s.log(p, "invite_remote_play", what, Result::ok);
}

bool SimStore::type_text(UserId user, const std::string& text) {
  SimPlayer* p = impl_->find(user);
  if (p == nullptr || p->keyboard_max == 0 || text.size() > p->keyboard_max) return false;
  p->keyboard_max = 0;
  Event e;
  e.kind = EventKind::text_entered;
  e.changed = true;
  e.text = text;
  impl_->queue(*p, std::move(e), p->now_us);
  impl_->journal.push_back("          store " + p->name + " typed '" + text + "'");
  return true;
}

bool SimStore::dismiss_keyboard(UserId user) {
  SimPlayer* p = impl_->find(user);
  if (p == nullptr || p->keyboard_max == 0) return false;
  p->keyboard_max = 0;
  Event e;
  e.kind = EventKind::text_entered;
  impl_->queue(*p, std::move(e), p->now_us);
  impl_->journal.push_back("          store " + p->name + " dismissed the keyboard");
  return true;
}

void SimStore::suspend(UserId user) {
  SimPlayer* p = impl_->find(user);
  if (p == nullptr || p->asleep) return;
  p->asleep = true;
  p->asleep_at_us = p->now_us;
  Event e;
  e.kind = EventKind::suspended;
  impl_->queue(*p, std::move(e), p->now_us);
  impl_->journal.push_back("          store " + p->name + " went to sleep");
}

void SimStore::resume(UserId user) {
  SimPlayer* p = impl_->find(user);
  if (p == nullptr || !p->asleep) return;
  p->asleep = false;
  Event e;
  e.kind = EventKind::resumed;
  e.total = static_cast<int32_t>((p->now_us - p->asleep_at_us) / 1000000);
  impl_->queue(*p, std::move(e), p->now_us);
  impl_->journal.push_back("          store " + p->name + " woke after " + std::to_string(e.total) +
                           " s");
}

bool SimStore::remote_join(UserId host, UserId friend_id, RemoteDevice device) {
  SimPlayer* p = impl_->find(host);
  const SimPlayer* f = impl_->find(friend_id);
  if (p == nullptr || f == nullptr) return false;
  const auto it = std::find(p->remote_invited.begin(), p->remote_invited.end(), friend_id);
  if (it == p->remote_invited.end()) return false;
  p->remote_invited.erase(it);
  p->remote.push_back({friend_id, f->name, device});
  Event e;
  e.kind = EventKind::remote_player_joined;
  e.user = friend_id;
  e.text = f->name;
  impl_->queue(*p, std::move(e), p->now_us);
  impl_->journal.push_back("          store " + f->name + " joined " + p->name + "'s remote play");
  return true;
}

bool SimStore::remote_leave(UserId host, UserId friend_id) {
  SimPlayer* p = impl_->find(host);
  if (p == nullptr) return false;
  const auto it = std::find_if(p->remote.begin(), p->remote.end(),
                               [&](const RemotePlayer& r) { return r.user == friend_id; });
  if (it == p->remote.end()) return false;
  Event e;
  e.kind = EventKind::remote_player_left;
  e.user = friend_id;
  e.text = it->name;
  impl_->journal.push_back("          store " + it->name + " left " + p->name + "'s remote play");
  p->remote.erase(it);
  impl_->queue(*p, std::move(e), p->now_us);
  return true;
}

std::vector<TimelineMark> SimStore::timeline(UserId user) const {
  const SimPlayer* p = impl_->find(user);
  return p ? p->timeline : std::vector<TimelineMark>{};
}

}  // namespace ge::services

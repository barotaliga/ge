// The matchmaker, defined (matchmaker.hpp).
//
// ge:summary The matchmaker defined: the queue kept, the window widened, pairs on offered seats
// ge:keywords matchmaker matchmaker_pair queue_window
// ge:skill net
// ge:test tests/account_matchmaker_test.cpp
#include "account/matchmaker.hpp"

#include <algorithm>

namespace ge::account {

namespace {

int64_t rating_on(const Ladder& l, std::string_view game, std::string_view mode, LadderKind kind,
                  uint64_t who) {
  const auto t = l.tables.find({std::string(game), std::string(mode), kind});
  if (t == l.tables.end()) return kRatingStart;
  const auto s = t->second.all_time.find(who);
  return s == t->second.all_time.end() ? kRatingStart : s->second.rating;
}

bool holds(const QueueEntry& e, uint64_t account) {
  return account != 0 && (e.account == account || e.partner == account);
}

bool shares(const QueueEntry& a, const QueueEntry& b) {
  return holds(b, a.account) || holds(b, a.partner);
}

int64_t gap(int64_t a, int64_t b) { return a > b ? a - b : b - a; }

}  // namespace

const char* queue_answer_words(QueueAnswer a) {
  switch (a) {
    case QueueAnswer::queued:
      return "Looking for a game.";
    case QueueAnswer::no_duo:
      return "You are not in a duo by that name. Make one with your partner first.";
    case QueueAnswer::bad_mode:
      return "That mode has no ranked games.";
    case QueueAnswer::full:
      return "Too many players are waiting right now. Try again in a minute.";
  }
  return "";
}

bool mode_is_pairs(std::string_view mode) {
  return mode == "2on2" || mode == "2v2" || mode == "tdm2";
}

int64_t queue_window(int64_t since_s, int64_t now_s) {
  const int64_t steps = std::max<int64_t>(0, now_s - since_s) / kWindowEvery_s;
  return std::min(kWindowMost, kWindowStart + steps * kWindowStep);
}

QueueAnswer matchmaker_join(Matchmaker& mm, const Ladder& ladder, uint64_t account,
                            std::string_view game, std::string_view mode, std::string_view duo,
                            int64_t now_s) {
  if (mode.empty() || game.empty()) return QueueAnswer::bad_mode;
  QueueEntry e;
  e.account = account;
  e.game = std::string(game);
  e.mode = std::string(mode);
  if (mode_is_pairs(mode)) {
    const Duo* d = nullptr;
    for (const Duo& one : ladder.duos)
      if (one.name == duo && (one.a == account || one.b == account)) d = &one;
    if (d == nullptr) return QueueAnswer::no_duo;
    e.duo = d->id;
    e.partner = d->a == account ? d->b : d->a;
    e.rating = rating_on(ladder, game, mode, LadderKind::pair, d->id);
  } else {
    e.rating = rating_on(ladder, game, mode, LadderKind::duel, account);
  }
  for (QueueEntry& q : mm.queue)
    if (holds(q, account) && q.game == e.game && q.mode == e.mode && q.duo == e.duo) {
      q.asked_s = now_s;  // asked again: its place kept
      return QueueAnswer::queued;
    }
  matchmaker_leave(mm, account);  // another game or duo asked for: that one goes
  if (e.partner != 0) matchmaker_leave(mm, e.partner);
  if (mm.queue.size() >= kQueueMost) return QueueAnswer::full;
  e.since_s = e.asked_s = now_s;
  mm.queue.push_back(std::move(e));
  return QueueAnswer::queued;
}

bool matchmaker_leave(Matchmaker& mm, uint64_t account) {
  return std::erase_if(mm.queue, [&](const QueueEntry& q) { return holds(q, account); }) > 0;
}

void matchmaker_offer(Matchmaker& mm, const net::Key& server, std::string_view game,
                      std::string_view mode, std::string_view address, int seats, int64_t now_s) {
  std::erase_if(mm.offers, [&](const Offer& o) {
    return o.server == server && o.game == game && o.mode == mode;
  });
  if (seats <= 0) return;
  Offer o;
  o.server = server;
  o.game = std::string(game);
  o.mode = std::string(mode);
  o.address = std::string(address);
  o.seats = seats;
  o.made_s = now_s;
  mm.offers.push_back(std::move(o));
}

std::vector<uint64_t> matchmaker_pair(Matchmaker& mm, int64_t now_s) {
  std::erase_if(mm.queue, [&](const QueueEntry& q) { return now_s - q.asked_s > kQueueLife_s; });
  std::erase_if(mm.offers, [&](const Offer& o) { return now_s - o.made_s > kOfferLife_s; });
  std::erase_if(mm.made, [&](const MadeMatch& m) { return now_s - m.made_s > kMatchKept_s; });
  std::vector<uint64_t> out;
  std::vector<bool> taken(mm.queue.size(), false);
  for (size_t i = 0; i < mm.queue.size(); i++) {
    if (taken[i]) continue;
    const QueueEntry& a = mm.queue[i];
    const int need = a.partner != 0 ? 4 : 2;
    Offer* best_offer = nullptr;
    for (Offer& o : mm.offers)
      if (o.game == a.game && o.mode == a.mode && o.seats >= need &&
          (best_offer == nullptr || o.seats > best_offer->seats))
        best_offer = &o;
    if (best_offer == nullptr) continue;
    const int64_t wa = queue_window(a.since_s, now_s);
    size_t best = mm.queue.size();
    int64_t best_gap = 0;
    for (size_t j = i + 1; j < mm.queue.size(); j++) {
      if (taken[j]) continue;
      const QueueEntry& b = mm.queue[j];
      if (b.game != a.game || b.mode != a.mode || shares(a, b)) continue;
      const int64_t d = gap(a.rating, b.rating);
      if (d > wa || d > queue_window(b.since_s, now_s)) continue;
      if (best == mm.queue.size() || d < best_gap) {
        best = j;
        best_gap = d;
      }
    }
    if (best == mm.queue.size()) continue;
    const QueueEntry& b = mm.queue[best];
    MadeMatch m;
    m.id = mm.next_id++;
    m.game = a.game;
    m.mode = a.mode;
    m.address = best_offer->address;
    m.side_a = {a.account};
    if (a.partner != 0) m.side_a.push_back(a.partner);
    m.side_b = {b.account};
    if (b.partner != 0) m.side_b.push_back(b.partner);
    m.rating_a = a.rating;
    m.rating_b = b.rating;
    m.made_s = now_s;
    best_offer->seats -= need;
    taken[i] = taken[best] = true;
    out.push_back(m.id);
    mm.made.push_back(std::move(m));
    mm.paired++;
  }
  size_t k = 0;
  std::erase_if(mm.queue, [&](const QueueEntry&) { return taken[k++]; });
  return out;
}

Look matchmaker_look(const Matchmaker& mm, uint64_t account, int64_t now_s) {
  Look l;  // queued again after a match: the wait is the news
  for (const QueueEntry& q : mm.queue)
    if (holds(q, account)) {
      l.state = LookState::waiting;
      l.waited_s = now_s - q.since_s;
      l.window = queue_window(q.since_s, now_s);
      return l;
    }
  for (auto it = mm.made.rbegin(); it != mm.made.rend(); ++it) {
    const auto in = [&](const std::vector<uint64_t>& side) {
      return std::find(side.begin(), side.end(), account) != side.end();
    };
    if (in(it->side_a) || in(it->side_b)) {
      l.state = LookState::matched;
      l.match = &*it;
      return l;
    }
  }
  return l;
}

}  // namespace ge::account

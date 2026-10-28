// The workshop on engine/services/sim_store.hpp: an item made and found; a draft past a store's
// limits refused; a subscription's download landing after the latency and the item's size at the
// store's speed, its files read as content; an update reaching every subscriber's machine only
// when its download lands; an unsubscribe taking the files; who sees what (friends-only, the
// author's own); the orders, the tag filter and the pages; and the offline store keeping items
// and subscriptions across a reopen.
//
// ge:summary Test: the workshop on the simulated and offline stores
// ge:keywords test services workshop item submit query subscribe download install update
// ge:keywords visibility page offline
// ge:skill services testing
#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "services/sim_store.hpp"

using namespace ge::services;
namespace fs = std::filesystem;

namespace {

constexpr std::array<StatDef, 1> kStats{{{"RUNS", StatKind::integer, 0, 1000000, true}}};
const Catalog kCatalog{3001, {}, kStats, {}, {}};

ItemDraft draft(std::string title, std::vector<std::string> tags = {"map"},
                ItemVisibility vis = ItemVisibility::everyone) {
  ItemDraft d;
  d.title = std::move(title);
  d.description = "A run down the north face";
  d.tags = std::move(tags);
  d.visibility = vis;
  d.change_note = "first";
  return d;
}

std::vector<ItemFile> files(size_t bytes, uint8_t fill) {
  return {{"level.bin", std::vector<uint8_t>(bytes, fill)}, {"readme.txt", {'h', 'i'}}};
}

std::vector<Event> poll(Backend& b, int64_t t) {
  std::vector<Event> ev;
  b.poll(t, ev);
  return ev;
}

}  // namespace

TEST(an_item_is_made_found_and_a_bad_draft_refused) {
  SimConfig config;
  config.latency_us = 1000;
  config.item_max_bytes = 4096;
  SimStore store(kCatalog, config);
  const UserId ana = store.add_user("ana"), bo = store.add_user("bo");
  auto a = store.backend(ana), b = store.backend(bo);
  const auto f = files(100, 1);
  const RequestId r = a->submit_item(0, draft("North face"), f);
  std::vector<Event> ev = poll(*a, 1000);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::item_submitted && ev[0].request == r &&
        ev[0].result == Result::ok && ev[0].item != 0 && ev[0].total == 1);
  const ItemId id = ev[0].item;
  b->query_items({});
  ev = poll(*b, 2000);
  CHECK(ev.size() == 1 && ev[0].total == 1 && ev[0].items[0].id == id &&
        ev[0].items[0].author == ana && ev[0].items[0].bytes == 102 &&
        ev[0].items[0].title == "North face" && !ev[0].items[0].subscribed);

  // Every limit a store holds an item to.
  int64_t t = 1000000;
  const auto refused = [&](Backend& who, ItemId item, const ItemDraft& d,
                           const std::vector<ItemFile>& fs_, Result want) {
    who.submit_item(item, d, fs_);
    t += 10000;
    const std::vector<Event> got = poll(who, t);
    CHECK(got.size() == 1 && got[0].result == want);
  };
  refused(*a, 0, draft(""), f, Result::invalid);
  refused(*a, 0, draft(std::string(129, 't')), f, Result::invalid);
  refused(*a, 0, draft("x", std::vector<std::string>(kMaxItemTags + 1, "t")), f, Result::invalid);
  refused(*a, 0, draft("x"), {{"../up", {1}}}, Result::invalid);
  refused(*a, 0, draft("x"), {{"a", {1}}, {"a", {2}}}, Result::invalid);
  refused(*a, 0, draft("x"), {}, Result::invalid);
  refused(*a, 0, draft("x"), files(5000, 1), Result::over_quota);
  refused(*b, id, draft("Mine now"), f, Result::invalid);  // not the author
  refused(*a, 999, draft("x"), f, Result::not_found);
  store.fail_next(ana, Result::rate_limited);
  refused(*a, 0, draft("x"), f, Result::rate_limited);
}

TEST(a_subscription_downloads_an_update_reaches_every_machine_and_unsubscribing_removes_it) {
  SimConfig config;
  config.latency_us = 50000;
  config.download_bytes_per_s = 1000000;  // 1 MB/s: a 500 kB item takes half a second
  SimStore store(kCatalog, config);
  const UserId ana = store.add_user("ana"), bo = store.add_user("bo"), cy = store.add_user("cy");
  auto a = store.backend(ana), b = store.backend(bo), c = store.backend(cy);
  a->submit_item(0, draft("North face"), files(500000 - 2, 7));
  const ItemId id = poll(*a, 50000)[0].item;

  b->subscribe_item(id, true);
  c->subscribe_item(id, true);
  std::vector<Event> ev = poll(*b, 50000);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::item_subscribed && ev[0].changed);
  std::vector<CloudFile> listing;
  CHECK(b->item_files(id, listing) == Result::not_found);  // still downloading
  ev = poll(*b, 549999);
  CHECK(ev.empty());
  ev = poll(*b, 550000);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::item_installed && ev[0].item == id &&
        ev[0].total == 1);
  CHECK(b->item_files(id, listing) == Result::ok && listing.size() == 2 &&
        listing[0].name == "level.bin" && listing[0].size == 499998);
  std::vector<uint8_t> bytes;
  CHECK(b->item_read(id, "level.bin", bytes) == Result::ok && bytes[0] == 7);
  CHECK(b->item_read(id, "missing", bytes) == Result::not_found);
  std::vector<ItemInfo> mine;
  b->subscribed_items(mine);
  CHECK(mine.size() == 1 && mine[0].installed == 1 && mine[0].subscribers == 2);

  // The author updates: each subscriber keeps version 1 until version 2 lands.
  a->submit_item(id, draft("North face, longer"), files(1000, 9));
  ev = poll(*a, 600000);
  CHECK(ev.size() == 1 && ev[0].total == 2);
  CHECK(b->item_read(id, "level.bin", bytes) == Result::ok && bytes[0] == 7);
  ev = poll(*b, 700000);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::item_installed && ev[0].total == 2);
  CHECK(b->item_read(id, "level.bin", bytes) == Result::ok && bytes.size() == 1000 &&
        bytes[0] == 9);
  ev = poll(*c, 700000);
  CHECK(ev.size() == 2 && ev[1].kind == EventKind::item_installed && ev[1].total == 2);

  // Unsubscribing takes the files, and a subscribe twice changes nothing.
  c->subscribe_item(id, true);
  ev = poll(*c, 800000);
  CHECK(ev.size() == 1 && !ev[0].changed);
  b->subscribe_item(id, false);
  ev = poll(*b, 800000);
  CHECK(ev.size() == 1 && ev[0].changed && b->item_files(id, listing) == Result::not_found);
  b->subscribed_items(mine);
  CHECK(mine.empty());
}

TEST(each_player_sees_only_what_they_may_and_queries_sort_filter_and_page) {
  SimConfig config;
  config.latency_us = 0;
  SimStore store(kCatalog, config);
  const UserId ana = store.add_user("ana"), bo = store.add_user("bo"), cy = store.add_user("cy");
  store.befriend(ana, bo);
  auto a = store.backend(ana), b = store.backend(bo), c = store.backend(cy);
  const auto f = files(10, 1);
  a->submit_item(0, draft("for friends", {"map"}, ItemVisibility::friends), f);
  a->submit_item(0, draft("just mine", {"map"}, ItemVisibility::author), f);
  poll(*a, 0);
  const auto count = [&](Backend& who, const ItemQuery& q) {
    who.query_items(q);
    return poll(who, 0)[0].total;
  };
  CHECK(count(*a, {}) == 2 && count(*b, {}) == 1 && count(*c, {}) == 0);
  c->subscribe_item(1, true);
  CHECK(poll(*c, 0)[0].result == Result::not_found);

  // 60 public items: the newest first, a tag, an author, the most subscribed, two pages.
  for (int i = 0; i < 60; i++)
    c->submit_item(0, draft("item " + std::to_string(i), {i % 2 ? "outfit" : "map"}), f);
  poll(*c, 0);
  b->query_items({});
  std::vector<Event> ev = poll(*b, 0);
  CHECK(ev[0].total == 61 && ev[0].items.size() == kItemsPerPage &&
        ev[0].items[0].title == "item 59");
  ItemQuery q;
  q.page = 2;
  b->query_items(q);
  ev = poll(*b, 0);
  CHECK(ev[0].items.size() == 11 && ev[0].items.back().title == "for friends");
  q = {};
  q.tag = "outfit";
  CHECK(count(*b, q) == 30);
  q = {};
  q.author = ana;
  CHECK(count(*b, q) == 1 && count(*a, q) == 2);
  b->subscribe_item(10, true);
  a->subscribe_item(10, true);
  poll(*a, 0);
  poll(*b, 0);
  q = {};
  q.order = ItemOrder::most_subscribed;
  b->query_items(q);
  ev = poll(*b, 0);
  CHECK(ev[0].items[0].id == 10 && ev[0].items[0].subscribers == 2);
  q = {};
  q.subscribed_only = true;
  CHECK(count(*b, q) == 1);
  q = {};
  q.page = 0;
  b->query_items(q);
  CHECK(poll(*b, 0)[0].result == Result::invalid);
}

TEST(the_offline_store_keeps_items_and_subscriptions) {
  const fs::path dir = fs::temp_directory_path() / "ge_services_workshop_test";
  fs::remove_all(dir);
  ItemId id = 0;
  {
    std::string error;
    Offline o = open_offline(kCatalog, dir.string(), "ana", &error);
    CHECK_MSG(o.store != nullptr, error);
    ItemDraft d = draft("Nordwand, 100% steep", {"map", "hard, very"});
    d.description = "Line one\nline two";
    o.backend->submit_item(0, d, files(300, 4));
    id = poll(*o.backend, 0)[0].item;
    o.backend->subscribe_item(id, true);
    poll(*o.backend, 1000000);
    std::vector<uint8_t> bytes;
    CHECK(o.backend->item_read(id, "level.bin", bytes) == Result::ok && bytes.size() == 300);
  }
  std::string error;
  Offline o = open_offline(kCatalog, dir.string(), "ana", &error);
  CHECK_MSG(o.store != nullptr, error);
  std::vector<ItemInfo> mine;
  o.backend->subscribed_items(mine);
  CHECK(mine.size() == 1 && mine[0].id == id && mine[0].title == "Nordwand, 100% steep" &&
        mine[0].description == "Line one\nline two" && mine[0].tags.size() == 2 &&
        mine[0].tags[1] == "hard, very" && mine[0].installed == 1);
  std::vector<uint8_t> bytes;
  CHECK(o.backend->item_read(id, "readme.txt", bytes) == Result::ok && bytes.size() == 2);
  o.backend->submit_item(0, draft("second"), files(1, 1));
  CHECK(poll(*o.backend, 0)[0].item == id + 1);  // the next id survived too
  fs::remove_all(dir);
}

// The workshop on the simulated store: items kept by the store with their latest files, queries
// that show each player only what they may see, subscriptions whose downloads land after the
// latency plus the item's size over `SimConfig::download_bytes_per_s`, and a new version sent to
// every subscriber's machine. The store holds an item to a store's limits (the title, the
// description, the tags, the file names, `SimConfig::item_max_bytes`) and refuses an update
// from anyone but its author.
//
// ge:summary The simulated store's workshop: items, queries, subscriptions, downloads, updates
// ge:keywords services sim store workshop ugc item query subscribe download install update
// ge:keywords visibility author friends version
// ge:skill services
// ge:test tests/services_workshop_test.cpp
#include <algorithm>
#include <cstddef>
#include <set>
#include <string>

#include "services/sim_store_state.hpp"

namespace ge::services {

namespace {

bool visible(const SimItem& item, const SimPlayer& p) {
  if (item.author == p.id) return true;
  if (item.draft.visibility == ItemVisibility::everyone) return true;
  if (item.draft.visibility == ItemVisibility::author) return false;
  return std::find(p.friends.begin(), p.friends.end(), item.author) != p.friends.end();
}

bool subscribed(const SimPlayer& p, ItemId id) {
  return std::find(p.subscriptions.begin(), p.subscriptions.end(), id) != p.subscriptions.end();
}

uint64_t size_of(std::span<const ItemFile> files) {
  uint64_t n = 0;
  for (const ItemFile& f : files) n += f.bytes.size();
  return n;
}

ItemInfo info_of(const SimStore::Impl& s, const SimItem& item, const SimPlayer& p) {
  ItemInfo i;
  i.id = item.id;
  i.author = item.author;
  i.title = item.draft.title;
  i.description = item.draft.description;
  i.tags = item.draft.tags;
  i.visibility = item.draft.visibility;
  i.version = item.version;
  i.bytes = size_of(item.files);
  for (const SimPlayer& q : s.players) i.subscribers += subscribed(q, item.id) ? 1 : 0;
  i.subscribed = subscribed(p, item.id);
  const auto it = p.installs.find(item.id);
  i.installed = it == p.installs.end() ? 0 : it->second.version;
  return i;
}

// What is wrong with a draft and its files ("" when nothing is).
std::string check_draft(const ItemDraft& d, std::span<const ItemFile> files) {
  if (d.title.empty() || d.title.size() > 128) return "title";
  if (d.description.size() > 8000) return "description";
  if (d.tags.size() > kMaxItemTags) return "tags";
  for (const std::string& t : d.tags)
    if (t.empty() || t.size() > 255) return "tag";
  if (files.empty()) return "no files";
  std::set<std::string> names;
  for (const ItemFile& f : files) {
    if (!cloud_name_ok(f.name) || !names.insert(f.name).second) return "file name " + f.name;
  }
  return {};
}

void forget_downloads(SimPlayer& p, ItemId id) {
  std::erase_if(p.downloads, [&](const SimDownload& d) { return d.item == id; });
  std::erase_if(p.queue, [&](const SimPending& q) {
    return q.event.kind == EventKind::item_installed && q.event.item == id;
  });
}

// Send `item`'s current version to `p`'s machine, starting at `start_us` and landing after the
// download; a version still on its way is overtaken.
void send_down(SimStore::Impl& s, SimPlayer& p, const SimItem& item, int64_t start_us) {
  forget_downloads(p, item.id);
  const uint64_t rate = std::max<uint64_t>(1, s.config.download_bytes_per_s);
  const int64_t due = start_us + static_cast<int64_t>(size_of(item.files) * 1000000 / rate);
  p.downloads.push_back({due, item.id, item.version, item.files});
  Event e;
  e.kind = EventKind::item_installed;
  e.item = item.id;
  e.total = static_cast<int32_t>(item.version);
  s.queue(p, std::move(e), due);
}

}  // namespace

RequestId workshop_query(SimStore::Impl& s, SimPlayer& p, const ItemQuery& q) {
  Event e;
  e.kind = EventKind::items_queried;
  e.request = p.next_request++;
  e.result = q.page == 0 ? Result::invalid : s.server_result(p);
  if (e.result == Result::ok) {
    std::vector<const SimItem*> hits;
    for (const SimItem& item : s.items) {
      if (!visible(item, p)) continue;
      if (q.author != 0 && item.author != q.author) continue;
      if (q.subscribed_only && !subscribed(p, item.id)) continue;
      if (!q.tag.empty() &&
          std::find(item.draft.tags.begin(), item.draft.tags.end(), q.tag) == item.draft.tags.end())
        continue;
      hits.push_back(&item);
    }
    std::vector<ItemInfo> infos;
    for (const SimItem* item : hits) infos.push_back(info_of(s, *item, p));
    std::stable_sort(infos.begin(), infos.end(), [&](const ItemInfo& a, const ItemInfo& b) {
      if (q.order == ItemOrder::most_subscribed && a.subscribers != b.subscribers)
        return a.subscribers > b.subscribers;
      return a.id > b.id;  // newest first
    });
    e.total = static_cast<int32_t>(infos.size());
    const size_t first = static_cast<size_t>(q.page - 1) * kItemsPerPage;
    for (size_t i = first; i < infos.size() && i < first + kItemsPerPage; i++)
      e.items.push_back(std::move(infos[i]));
  }
  s.log(p, "query_items",
        "page " + std::to_string(q.page) + " " + std::to_string(e.items.size()) + "/" +
            std::to_string(e.total),
        e.result);
  const RequestId r = e.request;
  s.queue(p, std::move(e), s.answer_time(p));
  return r;
}

RequestId workshop_submit(SimStore::Impl& s, SimPlayer& p, ItemId id, const ItemDraft& draft,
                          std::span<const ItemFile> files) {
  Event e;
  e.kind = EventKind::item_submitted;
  e.request = p.next_request++;
  SimItem* item = id == 0 ? nullptr : s.find_item(id);
  const std::string bad = check_draft(draft, files);
  if (id != 0 && item == nullptr)
    e.result = Result::not_found;
  else if (!bad.empty() || (item != nullptr && item->author != p.id))
    e.result = Result::invalid;
  else if (size_of(files) > s.config.item_max_bytes)
    e.result = Result::over_quota;
  else
    e.result = s.server_result(p);
  if (e.result == Result::ok) {
    if (item == nullptr) {
      s.items.push_back({});
      item = &s.items.back();
      item->id = s.next_item++;
      item->author = p.id;
    }
    item->draft = draft;
    item->version++;
    item->files.assign(files.begin(), files.end());
    std::sort(item->files.begin(), item->files.end(),
              [](const ItemFile& a, const ItemFile& b) { return a.name < b.name; });
    e.item = item->id;
    e.total = static_cast<int32_t>(item->version);
    for (SimPlayer& q : s.players)
      if (subscribed(q, item->id)) send_down(s, q, *item, s.answer_time(q));
    s.save_if_asked();
  }
  s.log(p, "submit_item",
        (id == 0 ? std::string("new") : std::to_string(id)) + " '" + draft.title + "'" +
            (bad.empty() ? std::string() : " (" + bad + ")"),
        e.result);
  const RequestId r = e.request;
  s.queue(p, std::move(e), s.answer_time(p));
  return r;
}

RequestId workshop_subscribe(SimStore::Impl& s, SimPlayer& p, ItemId id, bool subscribe) {
  Event e;
  e.kind = EventKind::item_subscribed;
  e.request = p.next_request++;
  e.item = id;
  const SimItem* item = s.find_item(id);
  e.result = item == nullptr || !visible(*item, p) ? Result::not_found : s.server_result(p);
  const RequestId r = e.request;
  const int64_t due = s.answer_time(p);
  if (e.result == Result::ok) {
    const bool was = subscribed(p, id);
    e.changed = subscribe != was;
    if (subscribe && !was) {
      p.subscriptions.push_back(id);
      const auto it = p.installs.find(id);
      if (it == p.installs.end() || it->second.version != item->version)
        send_down(s, p, *item, due);  // the download starts with the answer
    } else if (!subscribe && was) {
      std::erase(p.subscriptions, id);
      p.installs.erase(id);
      forget_downloads(p, id);
    }
    s.save_if_asked();
  }
  s.log(p, subscribe ? "subscribe_item" : "unsubscribe_item", std::to_string(id), e.result);
  s.queue(p, std::move(e), due);
  return r;
}

void workshop_subscribed(const SimStore::Impl& s, const SimPlayer& p, std::vector<ItemInfo>& out) {
  out.clear();
  for (const ItemId id : p.subscriptions)
    if (const SimItem* item = s.find_item(id)) out.push_back(info_of(s, *item, p));
}

Result workshop_files(const SimPlayer& p, ItemId item, std::vector<CloudFile>& out) {
  out.clear();
  const auto it = p.installs.find(item);
  if (it == p.installs.end()) return Result::not_found;
  for (const ItemFile& f : it->second.files) out.push_back({f.name, f.bytes.size()});
  return Result::ok;
}

Result workshop_read(const SimPlayer& p, ItemId item, std::string_view name,
                     std::vector<uint8_t>& out) {
  const auto it = p.installs.find(item);
  if (it == p.installs.end()) return Result::not_found;
  for (const ItemFile& f : it->second.files)
    if (f.name == name) {
      out = f.bytes;
      return Result::ok;
    }
  return Result::not_found;
}

void workshop_land(SimPlayer& p) {
  std::erase_if(p.downloads, [&](SimDownload& d) {
    if (d.due_us > p.now_us) return false;
    p.installs[d.item] = {d.version, std::move(d.files)};
    return true;
  });
}

}  // namespace ge::services

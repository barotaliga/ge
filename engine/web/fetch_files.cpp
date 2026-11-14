// A content pack's files fetched by hash, defined (fetch_files.hpp). Built only for Emscripten.
//
// ge:summary A pack's files fetched by hash defined: the browser's store, the check, the pack
// ge:keywords web fetch_listed cache-storage caches by-hash
// ge:skill net
#include "web/fetch_files.hpp"

#include <emscripten/emscripten.h>

#include <cstdlib>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>

#include "mod/sha256.hpp"
#include "vfs/pak.hpp"
#include "vfs/unpack.hpp"
#include "web/browser.hpp"
#include "web/file_list.hpp"

namespace {

// The URLs (one a line) the browser's store holds, read from it (`ge-content`, a secure page's
// Cache Storage; nothing elsewhere), or with `hub` fetched from the hub six at a time. 0 with one
// malloc'd block: each URL's 1 (its body follows: 4 bytes of length, little-endian, then the
// bytes) or 0 (not had), in order; the HTTP status of the first fetch that failed; -1 when one did
// not answer. With a label the status line counts the files fetched.
EM_ASYNC_JS(int, ge_web_take_files,
            (const char* urls, int hub, uint8_t** out, int* len, const char* label), {
              const list =
                  UTF8ToString(urls).split("\n").filter(function(u) { return u.length > 0; });
              const said = UTF8ToString(label);
              const line = document.getElementById("viewer-status");
              let store = null;
              try {
                if (!hub && globalThis.isSecureContext && globalThis.caches)
                  store = await caches.open("ge-content");
              } catch (e) {
                store = null;
              }
              const bodies = new Array(list.length);
              let done = 0, failed = 0, next = 0;
              async function one(i) {
                const url = list[i];
                if (!hub) {
                  if (!store) return;
                  try {
                    const r = await store.match(url);
                    if (r) bodies[i] = new Uint8Array(await r.arrayBuffer());
                  } catch (e) {
                  }
                  return;
                }
                let got;
                try {
                  got = await fetch(url);
                } catch (e) {
                  failed = failed || -1;
                  return;
                }
                if (!got.ok) {
                  failed = failed || got.status;
                  return;
                }
                bodies[i] = new Uint8Array(await got.arrayBuffer());
                done++;
                if (said && line)
                  line.textContent = said + " " + done + " of " + list.length + " files";
              }
              async function worker() {
                while (!failed && next < list.length) await one(next++);
              }
              const workers = [];
              for (let w = 0; w < 6; w++) workers.push(worker());
              await Promise.all(workers);
              if (failed) return failed;
              let total = 0;
              for (const b of bodies) total += b ? 5 + b.length : 1;
              const p = _malloc(total || 1);
              let at = p;
              for (const b of bodies) {
                HEAPU8[at] = b ? 1 : 0;
                at++;
                if (!b) continue;
                HEAPU8[at] = b.length & 255;
                HEAPU8[at + 1] = (b.length >> 8) & 255;
                HEAPU8[at + 2] = (b.length >> 16) & 255;
                HEAPU8[at + 3] = (b.length >>> 24) & 255;
                HEAPU8.set(b, at + 4);
                at += 4 + b.length;
              }
              HEAPU32[out >> 2] = p;
              HEAP32[len >> 2] = total;
              return 0;
            });

// A checked file kept in the browser's store under its URL, for the next visit.
EM_ASYNC_JS(void, ge_web_keep_file, (const char* url, const uint8_t* data, int len), {
  try {
    if (!globalThis.isSecureContext || !globalThis.caches) return;
    const store = await caches.open("ge-content");
    const b = HEAPU8.slice(data, data + len);
    await store.put(UTF8ToString(url),
                    new Response(b, {headers : {"Content-Type" : "application/octet-stream"}}));
  } catch (e) {
  }
});

using Got = std::vector<std::optional<std::vector<uint8_t>>>;

// `urls` taken from the store or the hub, one entry each (none when not had).
bool take(const std::string& urls, size_t count, bool hub, Got& out, std::string& error,
          const std::string& progress) {
  uint8_t* data = nullptr;
  int len = 0;
  const int status = ge_web_take_files(urls.c_str(), hub ? 1 : 0, &data, &len, progress.c_str());
  if (status != 0) {
    error = status < 0 ? "the hub did not answer for a file"
                       : "the hub answered " + std::to_string(status) + " for a file";
    return false;
  }
  out.assign(count, std::nullopt);
  const size_t size = static_cast<size_t>(len);
  size_t at = 0;
  for (size_t i = 0; i < count && at < size; i++) {
    if (data[at++] == 0) continue;
    uint32_t n = 0;
    if (at + 4 <= size) std::memcpy(&n, data + at, 4);  // little-endian, as wasm is
    if (at + 4 + n > size) break;
    out[i].emplace(data + at + 4, data + at + 4 + n);
    at += 4 + n;
  }
  std::free(data);
  return true;
}

bool is(const ge::web::ListedFile& f, const std::vector<uint8_t>& bytes) {
  return bytes.size() == f.bytes && ge::mod::sha256_hex(bytes.data(), bytes.size()) == f.sha;
}

}  // namespace

namespace ge::web {

bool fetch_listed(const std::string& list_url, FileStore& store, std::vector<uint8_t>& pak,
                  size_t& files, std::string& error, const std::string& progress) {
  pak.clear();
  files = 0;
  std::vector<uint8_t> text;
  if (!fetch_bytes(list_url, text, error)) return false;
  FileList list;
  if (!read_file_list({reinterpret_cast<const char*>(text.data()), text.size()}, list, error)) {
    error = list_url + ": " + error;
    return false;
  }
  std::vector<const ListedFile*> want;
  std::string urls;
  for (const ListedFile& f : list.files) {
    if (store.held.count(f.sha) != 0) continue;
    want.push_back(&f);
    urls += file_url(f.sha) + "\n";
  }
  if (want.empty()) return true;

  // What the browser kept, checked; a copy that is not its hash's is fetched again.
  Got kept;
  if (!take(urls, want.size(), false, kept, error, "")) return false;
  std::vector<std::vector<uint8_t>> bytes(want.size());
  std::vector<size_t> lack;
  uint64_t lack_bytes = 0, all_bytes = 0;
  for (size_t i = 0; i < want.size(); i++) {
    all_bytes += want[i]->bytes;
    if (kept[i] && is(*want[i], *kept[i])) {
      store.from_browser += kept[i]->size();
      bytes[i] = std::move(*kept[i]);
    } else {
      lack.push_back(i);
      lack_bytes += want[i]->bytes;
    }
  }

  // The rest from the hub: the whole pack when it lacks most of it (one pack packs smaller than
  // its files one by one), else each file it lacks.
  if (!lack.empty() && !list.pack.empty() && lack_bytes * 2 > all_bytes) {
    std::vector<uint8_t> packed;
    if (!fetch_bytes(pack_url(list.pack), packed, error, progress)) return false;
    store.from_hub += packed.size();
    ge::vfs::Unpacked u;
    ge::vfs::Pak dir;
    std::string why;
    if (!ge::vfs::unpack_one(std::move(packed), u, why) ||
        !ge::vfs::read_pak(u.bytes, &dir, &why)) {
      error = "the pack " + list.pack.substr(0, 12) + " does not read: " + why;
      return false;
    }
    for (size_t i : lack) {
      if (const ge::vfs::PakEntry* e = ge::vfs::pak_find(dir, want[i]->name)) {
        const std::span<const uint8_t> f = ge::vfs::pak_file(u.bytes, *e);
        bytes[i].assign(f.begin(), f.end());
      }
    }
  } else if (!lack.empty()) {
    std::string lack_urls;
    for (size_t i : lack) lack_urls += file_url(want[i]->sha) + "\n";
    Got got;
    if (!take(lack_urls, lack.size(), true, got, error, progress)) return false;
    for (size_t k = 0; k < lack.size(); k++) {
      if (!got[k]) continue;
      store.from_hub += got[k]->size();
      ge::vfs::Unpacked u;
      std::string why;
      if (ge::vfs::unpack_one(std::move(*got[k]), u, why)) bytes[lack[k]] = std::move(u.bytes);
    }
  }
  for (size_t i : lack) {
    const ListedFile& f = *want[i];
    if (!is(f, bytes[i])) {
      error = f.name + " (" + f.sha.substr(0, 12) + ") from the hub is not the file its hash names";
      return false;
    }
    const std::string url = file_url(f.sha);
    ge_web_keep_file(url.c_str(), bytes[i].data(), static_cast<int>(bytes[i].size()));
  }

  std::vector<std::pair<std::string, std::vector<uint8_t>>> named;
  for (size_t i = 0; i < want.size(); i++) {
    store.held.insert(want[i]->sha);
    named.emplace_back(want[i]->name, std::move(bytes[i]));
  }
  files = named.size();
  store.fetched += lack.size();
  return ge::vfs::write_pak(named, &pak, &error);
}

}  // namespace ge::web

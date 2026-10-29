// `ge source`'s body (source.hpp): the catalogue, curl into the cache, the pages scraped,
// the packs unpacked, SOURCE.txt written, each answered as text, findings and one JSON result.
//
// ge:summary ge source's body: the catalogue, scraping, downloading and unpacking packs
// ge:keywords cli source run_source kenney_slugs kenney_zip oga_results oga_page polyhaven_hits
// ge:keywords polyhaven_files curl unzip cache html-entities
// ge:skill agent-interface assets
// ge:test tests/cli_source_test.cpp
#include "cli/source.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>

#include "cli/build.hpp"
#include "cli/credits.hpp"
#include "cli/json.hpp"
#include "cli/json_tree.hpp"

namespace ge::cli {

namespace {

Outcome failed(const std::string& what, const std::string& where, const std::string& next,
               int exit = 1) {
  Outcome o;
  o.findings.push_back({Severity::error, what, where, next});
  o.exit = exit;
  return o;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::string entities(std::string_view s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] != '&') {
      out += s[i];
      continue;
    }
    const size_t semi = s.find(';', i);
    if (semi == std::string_view::npos || semi - i > 8) {
      out += '&';
      continue;
    }
    const std::string_view e = s.substr(i + 1, semi - i - 1);
    if (e == "amp") {
      out += '&';
    } else if (e == "lt") {
      out += '<';
    } else if (e == "gt") {
      out += '>';
    } else if (e == "quot") {
      out += '"';
    } else if (e == "#039" || e == "apos") {
      out += '\'';
    } else if (e == "nbsp") {
      out += ' ';
    } else {
      out += '&';
      continue;
    }
    i = semi;
  }
  return out;
}

// Every match of `open`...`close` after each `marker`, in order.
std::vector<std::string> between_all(std::string_view s, std::string_view marker,
                                     std::string_view open, std::string_view close) {
  std::vector<std::string> out;
  for (size_t at = s.find(marker); at != std::string_view::npos; at = s.find(marker, at + 1)) {
    const size_t a = s.find(open, at);
    if (a == std::string_view::npos) break;
    const size_t b = s.find(close, a + open.size());
    if (b == std::string_view::npos) break;
    out.emplace_back(s.substr(a + open.size(), b - a - open.size()));
  }
  return out;
}

// The HTML of one field block of an OpenGameArt page ("field-name-<name>" up to the next field).
std::string_view oga_field(std::string_view html, std::string_view name) {
  const size_t a = html.find("field-name-" + std::string(name));
  if (a == std::string_view::npos) return {};
  const size_t b = html.find("field-name-", a + 11);
  return html.substr(a, (b == std::string_view::npos ? html.size() : b) - a);
}

std::string url_words(const std::vector<std::string>& words) {
  std::string out;
  for (const std::string& w : words) {
    if (!out.empty()) out += '+';
    for (const unsigned char c : w) {
      if (std::isalnum(c) || c == '-' || c == '_') {
        out += static_cast<char>(c);
      } else {
        static constexpr char kHex[] = "0123456789ABCDEF";
        out += '%';
        out += kHex[c >> 4];
        out += kHex[c & 15];
      }
    }
  }
  return out;
}

fs::path cache_dir(const Args& a) {
  if (std::string c = flag_value(a, "cache"); !c.empty()) return c;
  if (const char* e = std::getenv("GE_ASSET_CACHE"); e && *e)
    return e;  // NOLINT(concurrency-mt-unsafe)
#ifdef _WIN32
  if (const char* e = std::getenv("LOCALAPPDATA"); e && *e) {  // NOLINT(concurrency-mt-unsafe)
    return fs::path(e) / "ge" / "sources";
  }
#endif
  if (const char* x = std::getenv("XDG_CACHE_HOME"); x && *x) {  // NOLINT(concurrency-mt-unsafe)
    return fs::path(x) / "ge" / "sources";
  }
  if (const char* h = std::getenv("HOME"); h && *h) {  // NOLINT(concurrency-mt-unsafe)
    return fs::path(h) / ".cache" / "ge" / "sources";
  }
  return fs::temp_directory_path() / "ge-sources";
}

bool download(const std::string& url, const fs::path& to) {
  std::error_code ec;
  fs::create_directories(to.parent_path(), ec);
  return run_quiet("curl -fsSL --retry 2 -o \"" + to.string() + "\" \"" + url + "\"") == 0;
}

bool fetch_text(const std::string& url, const fs::path& tmp, std::string& out) {
  if (!download(url, tmp)) return false;
  const std::optional<std::string> t = read_text(tmp);
  std::error_code ec;
  fs::remove(tmp, ec);
  if (!t) return false;
  out = *t;
  return true;
}

bool unpack(const fs::path& zip, const fs::path& dir) {
  if (run_quiet("unzip -qo \"" + zip.string() + "\" -d \"" + dir.string() + "\"") == 0) return true;
  return run_quiet("tar -xf \"" + zip.string() + "\" -C \"" + dir.string() + "\"") == 0;
}

struct Counts {
  int sounds = 0, models = 0, images = 0;
  fs::path first_model;  // the first .glb or .gltf, for the next command
};
Counts count_files(const fs::path& dir) {
  Counts c;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::end(it);
       it.increment(ec)) {
    const std::string e = lower(it->path().extension().string());
    if (e == ".wav" || e == ".ogg" || e == ".flac" || e == ".mp3") c.sounds++;
    if (e == ".glb" || e == ".gltf" || e == ".fbx" || e == ".obj") c.models++;
    if ((e == ".glb" || e == ".gltf") && c.first_model.empty()) c.first_model = it->path();
    if (e == ".png" || e == ".jpg") c.images++;
  }
  return c;
}

constexpr const char* kNetNext = "check the network and that curl is installed, then rerun";

struct Source {
  std::string_view id, licence, holds, search;
};
constexpr Source kSources[] = {
    {"kenney", "CC0 1.0, every pack",
     "low-poly 3D kits as GLB (weapons, props, dungeons, stations, small rigged characters), "
     "sound packs as OGG (impacts, interface, sci-fi, RPG, voices)",
     "words in the pack's name; --kind=sound or model narrows to Audio or 3D"},
    {"oga", "per pack: only CC0 is searched, and fetch reads the page's own licences",
     "sound effects, ambiences and 3D art by many authors",
     "words in the title, through the site's CC0 search; --kind=sound (default) or model"},
    {"polyhaven", "CC0 1.0, every asset",
     "photoscanned props as glTF with PBR textures (heavy: about 2 MB at 1k)",
     "words in the id, name or tags"},
};

Outcome run_list() {
  Outcome o;
  std::vector<std::string> rows;
  for (const Source& s : kSources) {
    o.text += "  " + std::string(s.id) + ": " + std::string(s.holds) +
              "\n    licence: " + std::string(s.licence) +
              "\n    search: " + std::string(s.search) + "\n";
    rows.push_back(json_obj({{"id", json_str(s.id)},
                             {"licence", json_str(s.licence)},
                             {"holds", json_str(s.holds)},
                             {"search", json_str(s.search)}}));
  }
  o.result = json_arr(rows);
  o.summary = std::to_string(std::size(kSources)) + " sources";
  o.next = "ge source search kenney crate --kind=model";
  return o;
}

Outcome hits_outcome(const std::vector<PackHit>& hits, const std::string& source) {
  Outcome o;
  std::vector<std::string> rows;
  for (const PackHit& h : hits) {
    o.text += "  " + h.id + (h.title.empty() ? "" : "  " + h.title) + "\n";
    rows.push_back(json_obj({{"id", json_str(h.id)}, {"title", json_str(h.title)}}));
  }
  o.result = json_arr(rows);
  o.summary = std::to_string(hits.size()) + " pack(s) on " + source;
  if (!hits.empty()) o.next = "ge source fetch " + hits[0].id;
  return o;
}

Outcome search_kenney(const Args& a, const std::vector<std::string>& words, const fs::path& cache) {
  const std::string kind = flag_value(a, "kind");
  const std::string cat = kind == "sound"   ? "category:Audio/"
                          : kind == "model" ? "category:3D/"
                                            : "";
  const fs::path index =
      cache / "kenney" / ("index-" + (cat.empty() ? std::string("all") : kind) + ".txt");
  std::vector<std::string> slugs;
  if (const auto t = read_text(index); t && !has_flag(a, "refresh")) {
    for (std::string_view l : split_lines(*t)) {
      if (!l.empty()) slugs.emplace_back(l);
    }
  } else {
    std::set<std::string> seen;
    for (int page = 1; page <= 60; page++) {
      std::string html;
      if (!fetch_text("https://kenney.nl/assets/" + cat + "page:" + std::to_string(page),
                      cache / "kenney" / "page.html", html)) {
        if (page == 1) return failed("could not read kenney.nl's listing", "kenney.nl", kNetNext);
        break;
      }
      size_t added = 0;
      for (std::string& s : kenney_slugs(html)) {
        if (seen.insert(s).second) {
          slugs.push_back(s);
          added++;
        }
      }
      if (!added) break;
    }
    std::string text;
    for (const std::string& s : slugs) text += s + "\n";
    std::string error;
    write_text(index, text, error);
  }
  std::vector<PackHit> hits;
  for (const std::string& s : slugs) {
    if (std::all_of(words.begin(), words.end(),
                    [&](const std::string& w) { return s.find(lower(w)) != std::string::npos; })) {
      hits.push_back({"kenney:" + s, ""});
    }
  }
  return hits_outcome(hits, "kenney (" + std::to_string(slugs.size()) + " packs listed)");
}

Outcome search_oga(const Args& a, const std::vector<std::string>& words, const fs::path& cache) {
  const std::string type = flag_value(a, "kind") == "model" ? "10" : "13";
  std::string html;
  const std::string url = "https://opengameart.org/art-search-advanced?keys=" + url_words(words) +
                          "&field_art_type_tid%5B%5D=" + type +
                          "&field_art_licenses_tid%5B%5D=4&sort_by=count&sort_order=DESC";
  if (!fetch_text(url, cache / "oga" / "search.html", html)) {
    return failed("could not search opengameart.org", url, kNetNext);
  }
  return hits_outcome(oga_results(html), "opengameart (CC0 only)");
}

Outcome search_polyhaven(const std::vector<std::string>& words, const fs::path& cache) {
  std::string json;
  if (!fetch_text("https://api.polyhaven.com/assets?t=models", cache / "polyhaven" / "index.json",
                  json)) {
    return failed("could not read Poly Haven's list", "api.polyhaven.com", kNetNext);
  }
  return hits_outcome(polyhaven_hits(json, words), "polyhaven");
}

Outcome fetched(const fs::path& dir, const std::string& id, const std::string& title,
                const std::string& author, const std::string& page, const std::string& licence) {
  std::string error;
  write_text(dir / "SOURCE.txt",
             "id: " + id + "\ntitle: " + title + "\nauthor: " + author + "\npage: " + page +
                 "\nlicence: " + licence + "\n",
             error);
  const Counts c = count_files(dir);
  Outcome o;
  o.text = "  " + id + " into " + dir.string() + ": " + std::to_string(c.sounds) + " sound(s), " +
           std::to_string(c.models) + " model(s), " + std::to_string(c.images) + " image(s)\n" +
           "  " + title + " by " + author + ", " + licence + "\n";
  o.result = json_obj({{"id", json_str(id)},
                       {"dir", json_str(dir.string())},
                       {"title", json_str(title)},
                       {"author", json_str(author)},
                       {"page", json_str(page)},
                       {"licence", json_str(licence)},
                       {"sounds", json_num(c.sounds)},
                       {"models", json_num(c.models)},
                       {"images", json_num(c.images)}});
  o.summary = "fetched " + id;
  o.next = c.sounds ? "ge sound match <reference> \"" + dir.string() + "\""
                    : "ge model import \"" +
                          (c.first_model.empty() ? (dir / "<file>.glb").string()
                                                 : c.first_model.string()) +
                          "\" <our/name>";
  return o;
}

Outcome fetch_kenney(const std::string& slug, const fs::path& cache) {
  const fs::path dir = cache / "kenney" / slug;
  const std::string page = "https://kenney.nl/assets/" + slug;
  std::string html;
  if (!fetch_text(page, cache / "kenney" / "pack.html", html)) {
    return failed("could not read " + page, page, kNetNext);
  }
  const std::string zip = kenney_zip(html, slug);
  if (zip.empty()) {
    return failed("kenney.nl names no zip for " + slug, page,
                  "check the name with `ge source search kenney <word>`");
  }
  const fs::path file = cache / "kenney" / (slug + ".zip");
  if (!download(zip, file)) return failed("could not download " + zip, zip, kNetNext);
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (!unpack(file, dir))
    return failed("could not unpack " + file.string(), file.string(), "install unzip");
  fs::remove(file, ec);
  return fetched(dir, "kenney:" + slug, slug, "Kenney (kenney.nl)", page, "CC0 1.0");
}

Outcome fetch_oga(const std::string& slug, const fs::path& cache) {
  const fs::path dir = cache / "oga" / slug;
  const std::string page = "https://opengameart.org/content/" + slug;
  std::string html;
  if (!fetch_text(page, cache / "oga" / "pack.html", html)) {
    return failed("could not read " + page, page, kNetNext);
  }
  const OgaPage p = oga_page(html);
  std::string licence;
  bool carried = false;
  for (const std::string& l : p.licences) {
    licence += (licence.empty() ? "" : ", ") + l;
    carried = carried || licence_verdict(l) == Severity::note;
  }
  if (!carried) {
    return failed(slug + " is under " + (licence.empty() ? "no licence ge could read" : licence) +
                      ", which this repository cannot carry",
                  page, "pick a CC0 pack: `ge source search oga <word>` lists only those");
  }
  if (p.files.empty()) return failed(page + " lists no files", page, "open the page and look");
  std::error_code ec;
  fs::create_directories(dir, ec);
  for (const std::string& url : p.files) {
    std::string name = url.substr(url.find_last_of('/') + 1);
    const fs::path file = dir / name;
    if (!download(url, file)) return failed("could not download " + url, url, kNetNext);
    if (lower(file.extension().string()) == ".zip") {
      unpack(file, dir / file.stem());
      fs::remove(file, ec);
    }
  }
  return fetched(dir, "oga:" + slug, p.title, p.author + " (OpenGameArt)", page, licence);
}

Outcome fetch_polyhaven(const std::string& id, const fs::path& cache) {
  const fs::path dir = cache / "polyhaven" / id;
  std::string json;
  if (!fetch_text("https://api.polyhaven.com/files/" + id, cache / "polyhaven" / "files.json",
                  json)) {
    return failed("could not read Poly Haven's files for " + id, "api.polyhaven.com", kNetNext);
  }
  const auto files = polyhaven_files(json);
  if (files.empty()) {
    return failed("Poly Haven has no 1k glTF of " + id, "api.polyhaven.com",
                  "check the id with `ge source search polyhaven <word>`");
  }
  for (const auto& [rel, url] : files) {
    if (!download(url, dir / rel)) return failed("could not download " + url, url, kNetNext);
  }
  return fetched(dir, "polyhaven:" + id, id, "Poly Haven", "https://polyhaven.com/a/" + id,
                 "CC0 1.0");
}

}  // namespace

std::vector<std::string> kenney_slugs(std::string_view html) {
  std::vector<std::string> out;
  std::set<std::string> seen;
  constexpr std::string_view kAt = "kenney.nl/assets/";
  for (size_t at = html.find(kAt); at != std::string_view::npos; at = html.find(kAt, at + 1)) {
    size_t e = at + kAt.size();
    while (e < html.size() &&
           (std::islower(static_cast<unsigned char>(html[e])) ||
            std::isdigit(static_cast<unsigned char>(html[e])) || html[e] == '-')) {
      e++;
    }
    if (e < html.size() && html[e] == ':') continue;  // category:, series:, tag:, page:
    const std::string slug(html.substr(at + kAt.size(), e - at - kAt.size()));
    if (slug.empty() || slug == "category" || slug == "series" || slug == "tag") continue;
    if (seen.insert(slug).second) out.push_back(slug);
  }
  return out;
}

std::string kenney_zip(std::string_view html, std::string_view slug) {
  // The page links its previews under the same directory, so take the first link that runs to
  // ".zip" without leaving its attribute.
  const std::string head = "https://kenney.nl/media/pages/assets/" + std::string(slug) + "/";
  for (size_t a = html.find(head); a != std::string_view::npos; a = html.find(head, a + 1)) {
    const size_t end = html.find_first_of("\"' <>", a);
    const std::string_view url = html.substr(a, end == std::string_view::npos ? end : end - a);
    if (url.ends_with(".zip")) return std::string(url);
  }
  return "";
}

std::vector<PackHit> oga_results(std::string_view html) {
  std::vector<PackHit> out;
  for (const std::string& a : between_all(html, "art-preview-title", "<a href=\"", "</a>")) {
    const size_t q = a.find('"');
    if (q == std::string::npos || !a.starts_with("/content/")) continue;
    const size_t gt = a.find('>', q);
    out.push_back(
        {"oga:" + a.substr(9, q - 9), entities(a.substr(gt == std::string::npos ? q : gt + 1))});
  }
  return out;
}

OgaPage oga_page(std::string_view html) {
  OgaPage p;
  const std::vector<std::string> t = between_all(html, "<title>", "<title>", "</title>");
  if (!t.empty()) {
    std::string title = entities(t[0]);
    if (const size_t bar = title.find(" | "); bar != std::string::npos) title.resize(bar);
    p.title = title;
  }
  const std::vector<std::string> au =
      between_all(oga_field(html, "author-submitter"), "href=\"/users/", "\">", "</a>");
  if (!au.empty()) p.author = entities(au[0]);
  for (const std::string& l :
       between_all(oga_field(html, "field-art-licenses"), "license-name", ">", "</div>")) {
    p.licences.push_back(entities(l));
  }
  for (const std::string& f :
       between_all(oga_field(html, "field-art-files"), "<a href=\"", "<a href=\"", "\"")) {
    if (f.starts_with("https://opengameart.org/sites/default/files/")) p.files.push_back(f);
  }
  return p;
}

std::vector<PackHit> polyhaven_hits(std::string_view index_json,
                                    const std::vector<std::string>& words) {
  std::vector<PackHit> out;
  JsonNode doc;
  std::string error;
  if (!parse_json(index_json, doc, error) || doc.kind != JsonNode::Kind::object) return out;
  for (const auto& [id, v] : doc.members) {
    std::string hay = lower(id);
    const JsonNode* name = v.find("name");
    if (name) hay += " " + lower(name->text);
    if (const JsonNode* tags = v.find("tags")) {
      for (const JsonNode& t : tags->items) hay += " " + lower(t.text);
    }
    if (std::all_of(words.begin(), words.end(), [&](const std::string& w) {
          return hay.find(lower(w)) != std::string::npos;
        })) {
      out.push_back({"polyhaven:" + id, name ? name->text : ""});
    }
  }
  return out;
}

std::vector<std::pair<std::string, std::string>> polyhaven_files(std::string_view files_json) {
  std::vector<std::pair<std::string, std::string>> out;
  JsonNode doc;
  std::string error;
  if (!parse_json(files_json, doc, error)) return out;
  const JsonNode* g = doc.find("gltf");
  g = g ? g->find("1k") : nullptr;
  g = g ? g->find("gltf") : nullptr;
  const JsonNode* url = g ? g->find("url") : nullptr;
  if (!url) return out;
  out.emplace_back(url->text.substr(url->text.find_last_of('/') + 1), url->text);
  if (const JsonNode* inc = g->find("include")) {
    for (const auto& [rel, f] : inc->members) {
      if (const JsonNode* u = f.find("url")) out.emplace_back(rel, u->text);
    }
  }
  return out;
}

Outcome run_source(const Args& a) {
  const std::string name(a.command->name);
  if (name == "source list") return run_list();
  const fs::path cache = cache_dir(a);
  if (name == "source search") {
    const std::string src = a.words[0];
    const std::vector<std::string> words(a.words.begin() + 1, a.words.end());
    if (src == "kenney") return search_kenney(a, words, cache);
    if (src == "oga") return search_oga(a, words, cache);
    if (src == "polyhaven") return search_polyhaven(words, cache);
    return failed("no source \"" + src + "\"", "the command line",
                  "ge source list: kenney, oga or polyhaven", 2);
  }
  const std::string id = a.words[0];
  const size_t colon = id.find(':');
  const std::string src = id.substr(0, colon),
                    pack = colon == std::string::npos ? "" : id.substr(colon + 1);
  if (pack.empty() || pack.find_first_of("/\\.\"' ") != std::string::npos) {
    return failed("\"" + id + "\" is not <source>:<pack>", "the command line",
                  "pass an id a search printed: kenney:impact-sounds", 2);
  }
  if (src == "kenney") return fetch_kenney(pack, cache);
  if (src == "oga") return fetch_oga(pack, cache);
  if (src == "polyhaven") return fetch_polyhaven(pack, cache);
  return failed("no source \"" + src + "\"", "the command line",
                "ge source list: kenney, oga or polyhaven", 2);
}

}  // namespace ge::cli

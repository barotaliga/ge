// THE DISCOVERY INDEX. Every source file carries a `ge:` tag block in its leading comment
// (AGENTS.md § Finding things; the `write-code` skill owns the format). This script reads
// those blocks, checks them, writes docs/index.md from them, and answers keyword queries
// across the files, the skills, the skills' lessons and the porting map in one pass. It also
// holds the CLEAN-SLATE rule (AGENTS.md § Rules for every task): outside the porting working
// area, no tracked text names the reference code a module was carried over from.
//
//   node tools/agents-index.mjs --check        every file tagged, every tag valid; writes the index
//   node tools/agents-index.mjs --write        regenerate docs/index.md          (make index)
//
// docs/index.md is GENERATED and gitignored: every checkout writes its own (`make index`, and
// `make agents-check` on the way), so no two branches can conflict over it.
//   node tools/agents-index.mjs --find a b     files, skills and lessons matching ANY word
//                                                                          (make find Q="a b")
//
// ge:summary Checks the ge: tag blocks, writes docs/index.md, answers keyword queries
// ge:keywords index discovery metadata tags keywords find search ge-tags agents-check
// ge:skill write-code
import { execFileSync } from "node:child_process";
import { existsSync, lstatSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const ROOT = new URL("..", import.meta.url).pathname;
const INDEX = "docs/index.md";
const SKILLS = ".agents/skills";

// Which tracked files must carry a block. Everything an agent edits as code.
const TAGGED = [
  "engine/**.hpp", "engine/**.cpp", "plugins/**.hpp", "plugins/**.cpp", "games/**.hpp", "games/**.cpp",
  "tests/*.hpp", "tests/*.cpp", "tools/**.mjs", "tools/**.sh", "porting/**.mjs",
];
const REQUIRED = ["summary", "keywords", "skill"];
const KNOWN = new Set([...REQUIRED, "test"]);
// A keyword is a lower-case concept in the engine's own words, kebab or snake case
// (`value-noise`, `sample_field`). A reference implementation's identifiers belong in the
// porting map, never here.
const KEYWORD_RE = /^[a-z0-9][a-z0-9_-]*$/;

// THE WORKING AREA: porting-time material, deleted when porting ends. Only here (and in the
// golden data, whose first line may record the environment that wrote it) may the reference
// code be named. `.agents/skills/port/` is the working area's skill.
const WORKING = ["porting/", ".agents/skills/port/", "tests/golden/"];
const PORTING_MAP = "porting/map.md";
const RETIRED = "ora" + "cle"; // the old tag that named a file's reference
// What the rest of the tree must not say. Each pattern is spelled so it does not match itself.
const LEGACY = [
  [/\bgame[2-5]\b/i, "a reference game"],
  [/oss-game-frame[w]ork|\bthe frame[w]ork's\b/i, "the reference kit"],
  [/\bora[c]les?\b/i, "the reference implementation"],
  [/\btype[s]cript\b/i, "the reference's language"],
  [/\b\w+\.t[s]\b/, "a reference source file"],
  [/\bport(?:ed)? o[f]\b|\bported fro[m]\b/i, "a port's provenance"],
  [/\bqua[k]e|\bq[w]d?\b/i, "the classic game Shooter's Edge is compatible with"],
];
const ENGINE_TERMS = `Say it in the engine's own terms; a reference belongs in porting/ ` +
  `(${PORTING_MAP} for a file's reference).`;
const SCAN_LINES = 60;
const MAX_COLUMN = 100; // .clang-format's ColumnLimit

const errors = [];
const err = (file, msg) => errors.push(`${file}: ${msg}`);

function gitFiles(patterns) {
  const out = execFileSync("git", ["ls-files", "--cached", "--others", "--exclude-standard", ...patterns], {
    cwd: ROOT,
    encoding: "utf8",
  });
  return out.split("\n").filter((f) => f && existsSync(join(ROOT, f))).sort();
}
const trackedFiles = () => gitFiles(TAGGED);
const inWorkingArea = (f) => WORKING.some((w) => f.startsWith(w));

// The clean-slate scan: every tracked text file outside the working area, line by line.
function checkLegacy() {
  for (const file of gitFiles([])) {
    if (inWorkingArea(file)) continue;
    let text;
    try {
      if (lstatSync(join(ROOT, file)).isSymbolicLink()) continue;
      text = readFileSync(join(ROOT, file), "utf8");
    } catch {
      continue;
    }
    if (text.includes("\0")) continue; // binary
    text.split("\n").forEach((line, i) => {
      for (const [re, what] of LEGACY) {
        const m = line.match(re);
        if (m) err(`${file}:${i + 1}`, `names ${what} ("${m[0]}"). ${ENGINE_TERMS}`);
      }
    });
  }
}

// The porting map: | `engine file` | reference module | reference identifiers |, one row a
// file, sorted by path, every file existing. Absent once porting ends, and then not checked.
function readPortingMap() {
  const rows = [];
  if (!existsSync(join(ROOT, PORTING_MAP))) return rows;
  const lines = readFileSync(join(ROOT, PORTING_MAP), "utf8").split("\n");
  lines.forEach((line, i) => {
    const m = line.match(/^\| `([^`]+)` \|(.*)\|(.*)\|\s*$/);
    if (!m) return;
    const row = { file: m[1], reference: m[2].trim(), identifiers: m[3].trim(), line: i + 1 };
    const where = `${PORTING_MAP}:${row.line}`;
    if (!existsSync(join(ROOT, row.file))) err(where, `names ${row.file}, which does not exist. Move or delete the row.`);
    const prev = rows.at(-1);
    if (prev && prev.file >= row.file) {
      err(where, `${row.file} is ${prev.file === row.file ? "listed twice" : "out of order"}; keep one row a file, sorted by path.`);
    }
    rows.push(row);
  });
  return rows;
}

function skillNames() {
  return readdirSync(join(ROOT, SKILLS), { withFileTypes: true })
    .filter((d) => d.isDirectory())
    .map((d) => d.name)
    .sort();
}

// The `ge:` lines in a file's first SCAN_LINES lines, as { tag: [values...] }.
function readTags(file) {
  const tags = {};
  const lines = readFileSync(join(ROOT, file), "utf8").split("\n").slice(0, SCAN_LINES);
  for (const line of lines) {
    const m = line.match(/^\s*(?:\/\/|#)\s*ge:([a-z-]+)\s+(.*?)\s*$/);
    if (!m) continue;
    (tags[m[1]] ??= []).push(m[2]);
    // clang-format reflows a longer comment and the wrapped half loses its `ge:` prefix.
    if (line.length > MAX_COLUMN) {
      err(file, `ge:${m[1]} line is ${line.length} columns (cap ${MAX_COLUMN}). Wrap it by ` +
        `repeating the tag on the next line: the values of repeated tags are joined.`);
    }
  }
  return tags;
}

function parseFile(file, skills) {
  const t = readTags(file);
  const entry = { file, summary: "", keywords: [], skills: [], tests: [] };
  if (Object.keys(t).length === 0) {
    err(file, "no ge: tag block. Add ge:summary, ge:keywords and ge:skill lines to the file's " +
      "leading comment (the `write-code` skill, § Discovery tags), then run `make index`.");
    return entry;
  }
  for (const tag of Object.keys(t)) {
    if (tag === RETIRED) {
      err(file, `ge:${RETIRED} is retired: the engine's files do not name their reference. Delete the ` +
        `line and put the reference in ${PORTING_MAP} (a row for this file).`);
    } else if (!KNOWN.has(tag)) {
      err(file, `unknown tag ge:${tag}; the tags are ${[...KNOWN].join(", ")}.`);
    }
  }
  for (const tag of REQUIRED) {
    if (!t[tag]) err(file, `missing ge:${tag}. Add it to the leading comment (the \`write-code\` skill).`);
  }
  entry.summary = (t.summary ?? []).join(" ");
  entry.keywords = (t.keywords ?? []).join(" ").split(/[\s,]+/).filter(Boolean);
  for (const k of entry.keywords) {
    if (inWorkingArea(file)) continue;
    if (!KEYWORD_RE.test(k)) {
      err(file, `keyword "${k}" is not a lower-case concept (kebab or snake case). Write it in the ` +
        `engine's words; a reference identifier goes in ${PORTING_MAP}.`);
    }
  }
  entry.skills = (t.skill ?? []).join(" ").split(/\s+/).filter(Boolean);
  for (const s of entry.skills) {
    if (!skills.includes(s)) err(file, `ge:skill names "${s}", which is not a directory in ${SKILLS}/.`);
  }
  entry.tests = (t.test ?? []).join(" ").split(/\s+/).filter(Boolean);
  for (const p of entry.tests) {
    if (!existsSync(join(ROOT, p))) err(file, `ge:test "${p}" does not exist.`);
  }
  return entry;
}

const cell = (s) => s.replace(/\|/g, "\\|");
const code = (s) => "`" + s + "`";

function render(entries) {
  const out = [];
  out.push("# Index", "");
  out.push("GENERATED by `make index` (`tools/agents-index.mjs`) from the `ge:` tag block at the top");
  out.push("of every source file, and never committed (it is gitignored): `make index` and `make agents-check`");
  out.push("rewrite it in every checkout. Do not edit it: change the tags and regenerate.");
  out.push("", "To search the files, the skills and their lessons at once: `make find Q=\"word word\"`.", "");

  out.push("## Files", "");
  out.push("| File | What | Skill | Test |", "| --- | --- | --- | --- |");
  for (const e of entries) {
    out.push(`| ${code(e.file)} | ${cell(e.summary)} | ${e.skills.map(code).join(" ")} | ` +
      `${e.tests.map(code).join(" ")} |`);
  }

  out.push("", "## Keywords", "");
  const byKey = new Map();
  for (const e of entries) for (const k of e.keywords) (byKey.get(k) ?? byKey.set(k, new Set()).get(k)).add(e.file);
  out.push("| Keyword | Files |", "| --- | --- |");
  const keys = [...byKey.keys()].sort((a, b) => a.toLowerCase().localeCompare(b.toLowerCase()) || a.localeCompare(b));
  for (const k of keys) out.push(`| ${k} | ${[...byKey.get(k)].sort().map(code).join(" ")} |`);
  return out.join("\n") + "\n";
}

// --find: case-insensitive substring match of ANY query word against a file's path, summary
// and keywords; a skill's name and description; a lesson's title, scope and concepts; a
// porting map row's file, reference and identifiers.
function find(words, entries, skills, mapRows) {
  const q = words.map((w) => w.toLowerCase());
  const hit = (...texts) => q.some((w) => texts.some((t) => t.toLowerCase().includes(w)));
  let n = 0;
  console.log("FILES");
  for (const e of entries) {
    if (hit(e.file, e.summary, ...e.keywords)) {
      console.log(`  ${e.file}  — ${e.summary}  [${e.skills.join(", ")}]`);
      n++;
    }
  }
  console.log("SKILLS (load with the Skill tool, or read .agents/skills/<name>/SKILL.md)");
  for (const s of skills) {
    const text = readFileSync(join(ROOT, SKILLS, s, "SKILL.md"), "utf8");
    const desc = text.match(/^description:\s*"?(.*?)"?\s*$/m)?.[1] ?? "";
    if (hit(s, desc)) {
      console.log(`  ${s}  — ${desc.slice(0, 140)}${desc.length > 140 ? "…" : ""}`);
      n++;
    }
  }
  console.log("LESSONS (read them: node tools/skill-lessons.mjs <skill>)");
  for (const s of skills) {
    const dir = join(ROOT, SKILLS, s, ".lessons");
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir).filter((f) => f.endsWith(".md")).sort()) {
      const head = readFileSync(join(dir, f), "utf8").match(/^---\n([\s\S]*?)\n---/)?.[1] ?? "";
      const field = (k) => head.match(new RegExp(`^${k}:\\s*(.*)$`, "m"))?.[1] ?? "";
      if (hit(field("title"), field("scope"), field("concepts"))) {
        console.log(`  ${s}/.lessons/${f}  — ${field("title")}`);
        n++;
      }
    }
  }
  if (mapRows.length) console.log(`PORTING MAP (${PORTING_MAP}: the reference each ported file reproduces)`);
  for (const r of mapRows) {
    if (hit(r.file, r.reference, r.identifiers)) {
      console.log(`  ${r.file}  — ${r.reference.replace(/`/g, "")}  ${r.identifiers.replace(/`/g, "")}`);
      n++;
    }
  }
  if (n === 0) {
    console.log(`\nNothing matched ${words.join(", ")}. Try a synonym or the reference's identifier, ` +
      "then grep the tree. Once you find the file, add the word you first searched for to its " +
      "ge:keywords so the next search hits.");
  }
}

const args = process.argv.slice(2);
const mode = args[0] ?? "--check";
if (!["--check", "--write", "--find"].includes(mode)) {
  console.error(`agents-index: unknown mode "${mode}". Use --check, --write, or --find <words>.`);
  process.exit(2);
}
const skills = skillNames();
const entries = trackedFiles().map((f) => parseFile(f, skills));
const mapRows = readPortingMap();
if (mode === "--check") checkLegacy();

if (mode === "--find") {
  if (args.length < 2) {
    console.error('agents-index: --find needs words: node tools/agents-index.mjs --find noise heightfield');
    process.exit(2);
  }
  find(args.slice(1), entries, skills, mapRows);
  process.exit(0);
}

// The index is an official doc: the working area is searchable (--find) but not listed.
const text = render(entries.filter((e) => !inWorkingArea(e.file)));
writeFileSync(join(ROOT, INDEX), text);
if (mode === "--write") console.log(`agents-index: wrote ${INDEX} (${entries.length} files)`);
for (const e of errors) console.error(`agents-index: ${e}`);
if (errors.length) process.exit(1);
if (mode === "--check") console.log(`agents-index: ok (${entries.length} files tagged, ${INDEX} written)`);

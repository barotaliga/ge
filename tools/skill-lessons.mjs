// THE LESSON READER: the reading half of the `skill-reflection` skill. A lesson is one file,
// .agents/skills/<skill>/.lessons/<unix-timestamp>-<slug>.md, with front matter (title, date,
// and the optional scope and concepts filters) and the lesson in the body. One file per lesson
// means parallel sessions never conflict; this script is how the next session reads them back
// and how a reflection pass finds the stale, the oversized and the duplicated.
// The design follows oss-game-framework's `ogf-skill-lessons`, rewritten for this repository.
//
//   node tools/skill-lessons.mjs                    inventory: every skill, its lessons, its verdict
//   node tools/skill-lessons.mjs <skill>            that skill's lessons in full, oldest first
//   node tools/skill-lessons.mjs [<skill>] --list   index only: file, words, scope, concepts, title
//   node tools/skill-lessons.mjs --vocab            every concept in use, with counts
//   node tools/skill-lessons.mjs --check            validate every fragment and the size bars
//   Filters (with a print or --list):
//     --scope=engine/core/noise.hpp  lessons whose scope overlaps the path, plus every global one
//     --no-global                    drop the global ones (a consolidation sweep)
//     --concepts=a,b                 lessons carrying ANY of these concepts
//
// ge:summary Prints, filters and checks the skills' lesson fragments (make lessons)
// ge:keywords skill-reflection lessons fragments scope concepts vocab size-bars consolidation
// ge:skill skill-reflection
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";

const ROOT = new URL("..", import.meta.url).pathname;
const SKILLS = join(ROOT, ".agents/skills");

// The size bars, in words (`skill-reflection` § The size bars). Over one is a prompt to
// consolidate; --check reports them as warnings and fails only on format errors.
const LESSON_WORDS = 350;
const CORPUS_WORDS = 4000;
const MAX_LESSONS = 15;
const SKILL_WORDS = 5000;

const FILENAME_RE = /^\d{9,}-[a-z0-9][a-z0-9-]*\.md$/;
const DATE_RE = /^\d{4}-\d{2}-\d{2}$/;
const CONCEPT_RE = /^[a-z0-9][a-z0-9-]*$/;
const words = (s) => (s.trim().match(/\S+/g) ?? []).length;
const unquote = (v) => v.trim().replace(/^(["'])(.*)\1$/, "$2");
const list = (v) => (v ? v.replace(/^\[|\]$/g, "").split(",").map(unquote).filter(Boolean) : []);
const norm = (p) => p.replace(/^\.\//, "").replace(/\/+$/, "");
// Prefix in EITHER direction: a lesson scoped engine/core answers engine/core/noise.hpp and
// engine/ alike.
const overlaps = (a, b) => {
  const [x, y] = [norm(a), norm(b)];
  return x === y || x.startsWith(y + "/") || y.startsWith(x + "/");
};

function parse(skill, name) {
  const raw = readFileSync(join(SKILLS, skill, ".lessons", name), "utf8");
  const l = { skill, name, title: "", date: "", scope: [], concepts: [], body: "", errors: [], warnings: [] };
  if (!FILENAME_RE.test(name)) l.errors.push("filename must be <unix-timestamp>-<kebab-slug>.md ($(date +%s))");
  const m = raw.match(/^---\n([\s\S]*?)\n---\n?([\s\S]*)$/);
  if (!m) {
    l.errors.push("no front matter (--- title, date[, scope, concepts] ---)");
    l.body = raw.trim();
    return l;
  }
  const meta = {};
  for (const line of m[1].split("\n")) {
    const kv = line.match(/^(\w+):\s*(.*)$/);
    if (kv) meta[kv[1]] = unquote(kv[2]);
  }
  for (const k of Object.keys(meta)) {
    if (!["title", "date", "scope", "concepts"].includes(k)) l.errors.push(`unknown front-matter key "${k}"`);
  }
  l.title = meta.title ?? "";
  l.date = meta.date ?? "";
  if (!l.title) l.errors.push("no title (one line, written as a claim)");
  if (!DATE_RE.test(l.date)) l.errors.push(`date "${l.date}" is not YYYY-MM-DD`);
  l.scope = list(meta.scope);
  for (const p of l.scope) {
    if (p.startsWith("/") || p.includes("..")) l.errors.push(`scope "${p}" must be a repo-relative path`);
    // A vanished path is the best staleness signal there is: surfaced, not failed.
    else if (!existsSync(join(ROOT, p))) l.warnings.push(`scope "${p}" no longer exists: is the lesson stale?`);
  }
  l.concepts = list(meta.concepts);
  for (const c of l.concepts) if (!CONCEPT_RE.test(c)) l.errors.push(`concept "${c}" is not lower-case kebab`);
  l.body = m[2].trim();
  if (!l.body) l.errors.push("empty body");
  const n = words(l.body);
  if (n > LESSON_WORDS) l.warnings.push(`${n} words, over the ${LESSON_WORDS}-word bar: split, cut the story, or promote`);
  return l;
}

function skillNames() {
  return readdirSync(SKILLS, { withFileTypes: true }).filter((d) => d.isDirectory()).map((d) => d.name).sort();
}
function lessonsOf(skill) {
  const dir = join(SKILLS, skill, ".lessons");
  if (!existsSync(dir)) return [];
  return readdirSync(dir).filter((f) => f.endsWith(".md")).sort().map((f) => parse(skill, f));
}
function skillWarnings(skill, ls) {
  const w = [];
  const corpus = ls.reduce((s, l) => s + words(l.body), 0);
  if (ls.length > MAX_LESSONS) w.push(`${ls.length} lessons, over ${MAX_LESSONS}: run a consolidation sweep`);
  if (corpus > CORPUS_WORDS) w.push(`${corpus} lesson words, over ${CORPUS_WORDS}: merge and promote`);
  const md = join(SKILLS, skill, "SKILL.md");
  const sw = existsSync(md) ? words(readFileSync(md, "utf8")) : 0;
  if (sw > SKILL_WORDS) w.push(`SKILL.md is ${sw} words, over ${SKILL_WORDS}: cut what is said twice`);
  return w;
}

const args = process.argv.slice(2);
const flag = (k) => args.find((a) => a === k || a.startsWith(k + "="));
const value = (k) => flag(k)?.split("=").slice(1).join("=") ?? "";
const known = ["--list", "--vocab", "--check", "--scope", "--no-global", "--concepts", "--help"];
for (const a of args.filter((a) => a.startsWith("--"))) {
  if (!known.includes(a.split("=")[0])) {
    console.error(`skill-lessons: unknown flag ${a}. Flags: ${known.join(" ")}.`);
    process.exit(2);
  }
}
if (flag("--help")) {
  console.log(readFileSync(new URL(import.meta.url), "utf8").split("\n").filter((l) => l.startsWith("//")).slice(6, 17).join("\n"));
  process.exit(0);
}
const all = skillNames();
const named = args.find((a) => !a.startsWith("--"));
if (named && !all.includes(named)) {
  console.error(`skill-lessons: no skill "${named}". Skills: ${all.join(", ")}.`);
  process.exit(2);
}
const skills = named ? [named] : all;

if (flag("--check")) {
  let errors = 0;
  for (const s of skills) {
    const ls = lessonsOf(s);
    for (const l of ls) {
      for (const e of l.errors) (console.error(`ERROR ${s}/.lessons/${l.name}: ${e}`), errors++);
      for (const w of l.warnings) console.error(`warn  ${s}/.lessons/${l.name}: ${w}`);
    }
    for (const w of skillWarnings(s, ls)) console.error(`warn  ${s}: ${w}`);
  }
  if (errors) {
    console.error(`skill-lessons: ${errors} error(s). Fix the fragments (the \`skill-reflection\` skill, § The fragment format).`);
    process.exit(1);
  }
  console.log(`skill-lessons: ok (${skills.reduce((n, s) => n + lessonsOf(s).length, 0)} lessons)`);
  process.exit(0);
}

if (flag("--vocab")) {
  const count = new Map();
  for (const s of skills) for (const l of lessonsOf(s)) for (const c of l.concepts) count.set(c, (count.get(c) ?? 0) + 1);
  for (const [c, n] of [...count].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]))) console.log(`${String(n).padStart(3)}  ${c}`);
  process.exit(0);
}

const scopes = list(value("--scope"));
const concepts = list(value("--concepts"));
const keep = (l) =>
  (scopes.length === 0 || (l.scope.length === 0 ? !flag("--no-global") : l.scope.some((p) => scopes.some((q) => overlaps(p, q))))) &&
  (concepts.length === 0 || l.concepts.some((c) => concepts.includes(c)));

if (!named && !flag("--list") && scopes.length === 0 && concepts.length === 0) {
  // Inventory.
  for (const s of skills) {
    const ls = lessonsOf(s);
    const w = skillWarnings(s, ls);
    console.log(`${s.padEnd(18)} ${String(ls.length).padStart(3)} lessons  ${w.length ? "CONSOLIDATE: " + w.join("; ") : "ok"}`);
  }
  console.log("\nRead one: node tools/skill-lessons.mjs <skill>   (filters: --scope=<path> --concepts=a,b)");
  process.exit(0);
}

const picked = skills.flatMap(lessonsOf).filter(keep).sort((a, b) => a.name.localeCompare(b.name));
if (flag("--list")) {
  for (const l of picked) {
    console.log(`${l.skill}/${l.name}  ${words(l.body)}w  scope=${l.scope.join(",") || "(global)"}  concepts=${l.concepts.join(",")}\n    ${l.title}`);
  }
} else {
  for (const l of picked) {
    console.log(`## ${l.title}\n${l.skill}/.lessons/${l.name} · ${l.date} · scope ${l.scope.join(", ") || "global"}${l.concepts.length ? " · " + l.concepts.join(", ") : ""}\n\n${l.body}\n`);
  }
}
if (picked.length === 0) console.log("No lessons match. A skill with none is read from its SKILL.md alone; reflect into it at the end.");

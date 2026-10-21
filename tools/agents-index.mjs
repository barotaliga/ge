// THE DISCOVERY INDEX. Every source file carries a `ge:` tag block in its leading comment
// (AGENTS.md § Finding things; the `write-code` skill owns the format). This script reads
// those blocks, checks them, writes docs/index.md from them, and answers keyword queries
// across the files, the skills and the skills' lessons in one pass.
//
//   node tools/agents-index.mjs --check        every file tagged, every tag valid, index fresh
//   node tools/agents-index.mjs --write        regenerate docs/index.md          (make index)
//   node tools/agents-index.mjs --find a b     files, skills and lessons matching ANY word
//                                                                          (make find Q="a b")
//
// ge:summary Checks the ge: tag blocks, writes docs/index.md, answers keyword queries
// ge:keywords index discovery metadata tags keywords find search ge-tags agents-check
// ge:skill write-code
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const ROOT = new URL("..", import.meta.url).pathname;
const INDEX = "docs/index.md";
const SKILLS = ".agents/skills";

// Which tracked files must carry a block. Everything an agent edits as code.
const TAGGED = [
  "engine/**.hpp", "engine/**.cpp", "games/**.hpp", "games/**.cpp",
  "tests/*.hpp", "tests/*.cpp", "tools/**.mjs", "tools/**.sh",
];
const REQUIRED = ["summary", "keywords", "skill"];
const KNOWN = new Set([...REQUIRED, "oracle", "test"]);
const ORACLES = ["game5", "oss-game-framework"];
// A keyword is a lower-case kebab concept (`value-noise`) or an oracle identifier spelled as
// the TypeScript spells it (`sampleField`), so a grep for either name lands here.
const KEYWORD_RE = /^[A-Za-z][A-Za-z0-9_-]*$/;
const SCAN_LINES = 60;
const MAX_COLUMN = 100; // .clang-format's ColumnLimit

const errors = [];
const err = (file, msg) => errors.push(`${file}: ${msg}`);

function trackedFiles() {
  const out = execFileSync("git", ["ls-files", "--cached", "--others", "--exclude-standard", ...TAGGED], {
    cwd: ROOT,
    encoding: "utf8",
  });
  return out.split("\n").filter((f) => f && existsSync(join(ROOT, f))).sort();
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
  const entry = { file, summary: "", keywords: [], skills: [], oracles: [], tests: [] };
  if (Object.keys(t).length === 0) {
    err(file, "no ge: tag block. Add ge:summary, ge:keywords and ge:skill lines to the file's " +
      "leading comment (the `write-code` skill, § Discovery tags), then run `make index`.");
    return entry;
  }
  for (const tag of Object.keys(t)) {
    if (!KNOWN.has(tag)) err(file, `unknown tag ge:${tag}; the tags are ${[...KNOWN].join(", ")}.`);
  }
  for (const tag of REQUIRED) {
    if (!t[tag]) err(file, `missing ge:${tag}. Add it to the leading comment (the \`write-code\` skill).`);
  }
  entry.summary = (t.summary ?? []).join(" ");
  entry.keywords = (t.keywords ?? []).join(" ").split(/[\s,]+/).filter(Boolean);
  for (const k of entry.keywords) {
    if (!KEYWORD_RE.test(k)) err(file, `keyword "${k}" is not a kebab concept or an identifier.`);
  }
  entry.skills = (t.skill ?? []).join(" ").split(/\s+/).filter(Boolean);
  for (const s of entry.skills) {
    if (!skills.includes(s)) err(file, `ge:skill names "${s}", which is not a directory in ${SKILLS}/.`);
  }
  entry.oracles = (t.oracle ?? []).join(" ").split(/\s+/).filter(Boolean);
  for (const o of entry.oracles) {
    const repo = o.split("/")[0];
    if (!ORACLES.includes(repo)) {
      err(file, `ge:oracle "${o}" must start with ${ORACLES.join(" or ")}/ and name the oracle file.`);
    } else if (existsSync(join(ROOT, "..", repo)) && !existsSync(join(ROOT, "..", o))) {
      // Checked only where the oracle is cloned beside this checkout (never on CI).
      err(file, `ge:oracle "${o}" does not exist in ../${repo}. Fix the path, or the oracle moved.`);
    }
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
  out.push("of every source file. Do not edit by hand: change the tags and regenerate. `make agents-check`");
  out.push("fails when this page is stale. On a merge conflict here, take either side and run `make index`.");
  out.push("", "To search the files, the skills and their lessons at once: `make find Q=\"word word\"`.", "");

  out.push("## Files", "");
  out.push("| File | What | Skill | Oracle | Test |", "| --- | --- | --- | --- | --- |");
  for (const e of entries) {
    out.push(`| ${code(e.file)} | ${cell(e.summary)} | ${e.skills.map(code).join(" ")} | ` +
      `${e.oracles.map(code).join(" ")} | ${e.tests.map(code).join(" ")} |`);
  }

  out.push("", "## Oracle to port", "");
  out.push("Which C++ files port each oracle file. An oracle file missing here is not ported yet.", "");
  const byOracle = new Map();
  for (const e of entries) for (const o of e.oracles) (byOracle.get(o) ?? byOracle.set(o, []).get(o)).push(e.file);
  out.push("| Oracle | Port |", "| --- | --- |");
  for (const o of [...byOracle.keys()].sort()) out.push(`| ${code(o)} | ${byOracle.get(o).map(code).join(" ")} |`);

  out.push("", "## Keywords", "");
  const byKey = new Map();
  for (const e of entries) for (const k of e.keywords) (byKey.get(k) ?? byKey.set(k, new Set()).get(k)).add(e.file);
  out.push("| Keyword | Files |", "| --- | --- |");
  const keys = [...byKey.keys()].sort((a, b) => a.toLowerCase().localeCompare(b.toLowerCase()) || a.localeCompare(b));
  for (const k of keys) out.push(`| ${k} | ${[...byKey.get(k)].sort().map(code).join(" ")} |`);
  return out.join("\n") + "\n";
}

// --find: case-insensitive substring match of ANY query word against a file's path, summary
// and keywords; a skill's name and description; a lesson's title, scope and concepts.
function find(words, entries, skills) {
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
  if (n === 0) {
    console.log(`\nNothing matched ${words.join(", ")}. Try a synonym or the oracle's identifier, ` +
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

if (mode === "--find") {
  if (args.length < 2) {
    console.error('agents-index: --find needs words: node tools/agents-index.mjs --find noise heightfield');
    process.exit(2);
  }
  find(args.slice(1), entries, skills);
  process.exit(0);
}

const text = render(entries);
if (mode === "--write") {
  writeFileSync(join(ROOT, INDEX), text);
  console.log(`agents-index: wrote ${INDEX} (${entries.length} files)`);
} else {
  const current = existsSync(join(ROOT, INDEX)) ? readFileSync(join(ROOT, INDEX), "utf8") : "";
  if (current !== text) err(INDEX, "is stale against the ge: tags. Run `make index` and commit the result.");
}
for (const e of errors) console.error(`agents-index: ${e}`);
if (errors.length) process.exit(1);
if (mode === "--check") console.log(`agents-index: ok (${entries.length} files tagged, ${INDEX} fresh)`);

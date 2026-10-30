// THE ROADMAP's check and table (docs/goals.md § Milestones; run by `make agents-check` and
// `make index`). Each milestone row is its own file, docs/goals/<row>.md, so pull requests adding
// or updating different rows never touch the same file. A row file opens with a header of
// `Key: value` lines up to its first blank line. Holds:
//   - line 1 is `Status: done`, `Status: in progress` or `Status: open`;
//   - the header has a non-empty `Milestone:`, `Exit:` and `Labs:` line (other keys are kept);
//   - no committed file lists the rows: docs/goals.md has no milestone table.
// `--absorb` moves a pre-row-file branch's table edits into the files during a merge (below).
// Then writes docs/roadmap.md (gitignored, like docs/index.md): every row in id order, numbers
// compared as numbers, as one table. docs/goals/ge-backlog.md (the `ge` wishes, one a line) and
// docs/goals/reserved.md (rows promised before any branch carries them, read by `ge goals free`)
// are not rows.
//
// ge:summary Checks every roadmap row file's header and writes the generated table docs/roadmap.md
// ge:keywords goals roadmap milestones status check table status-file row-file generated
// ge:skill write-code
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const ROOT = new URL("..", import.meta.url).pathname;
const DIR = join(ROOT, "docs/goals");
const WORDS = ["done", "in progress", "open"];
const KEYS = ["Milestone", "Exit", "Labs"];
const FREE = new Set(["ge-backlog.md", "reserved.md"]);
const errors = [];
const err = (msg) => errors.push(msg);

// A row file's header: its `Key: value` lines up to the first blank line.
const header = (text) => {
  const out = new Map();
  for (const line of text.split("\n")) {
    if (line.trim() === "") break;
    const m = /^([A-Z][A-Za-z ]*): (.*)$/.exec(line);
    if (m) out.set(m[1], m[2].trim());
  }
  return out;
};

// Ids compared as runs of digits and of other characters, numbers as numbers: M8f9 < M8f11.
const runs = (id) => id.match(/\d+|\D+/g) ?? [];
const byId = (a, b) => {
  const x = runs(a);
  const y = runs(b);
  for (let k = 0; k < Math.min(x.length, y.length); k++) {
    if (x[k] === y[k]) continue;
    const nx = /^\d/.test(x[k]);
    const ny = /^\d/.test(y[k]);
    if (nx && ny) return Number(x[k]) - Number(y[k]);
    if (nx !== ny) return nx ? -1 : 1;
    return x[k] < y[k] ? -1 : 1;
  }
  return x.length - y.length;
};

// A table row's cells, split on the pipes outside backticks.
const cells = (line) => {
  const out = [];
  let cur = "";
  let tick = false;
  for (const ch of line.trim().slice(1, -1)) {
    if (ch === "`") tick = !tick;
    if (ch === "|" && !tick) {
      out.push(cur.trim());
      cur = "";
    } else cur += ch;
  }
  out.push(cur.trim());
  return out;
};

// A milestone table's rows (docs/goals.md before the rows had files): id → [Milestone, Exit, Labs].
const tableRows = (text) => {
  const lines = text.split("\n");
  const out = new Map();
  const start = lines.findIndex((l) => l.startsWith("| # | Milestone |"));
  for (let k = start + 2; start >= 0 && k < lines.length && lines[k].startsWith("| "); k++) {
    const c = cells(lines[k]);
    out.set(c[0], c.slice(1, 4).map((s) => s.replace(/\\\|/g, "|")));
  }
  return out;
};

// --absorb, while merging main into a branch from before the rows had files: the rows this
// branch added or changed in its docs/goals.md table (against the merge base) are written into
// their files, and docs/goals.md is taken from main. Rows the branch left alone keep main's text.
if (process.argv.includes("--absorb")) {
  const git = (args) => execFileSync("git", args, { cwd: ROOT, encoding: "utf8", stdio: ["ignore", "pipe", "ignore"] });
  const show = (ref) => {
    try {
      return git(["show", `${ref}:docs/goals.md`]);
    } catch {
      return "";
    }
  };
  let base;
  try {
    base = git(["merge-base", "HEAD", "MERGE_HEAD"]).trim();
  } catch {
    console.error("check-goals --absorb: no merge in progress; run it after `git merge origin/main` stops on docs/goals.md");
    process.exit(1);
  }
  const before = tableRows(show(base));
  const mine = tableRows(show("HEAD"));
  let moved = 0;
  for (const [id, row] of mine) {
    const was = before.get(id);
    if (was && was.every((s, k) => s === row[k])) continue;
    const file = join(DIR, `${id}.md`);
    const text = existsSync(file) ? readFileSync(file, "utf8") : "Status: open\n";
    const lines = text.split("\n");
    let end = lines.findIndex((l) => l.trim() === "");
    if (end < 0) end = lines.length;
    const head = lines.slice(0, end).filter((l) => !/^(Milestone|Exit|Labs): /.test(l));
    const body = [head[0], ...KEYS.map((k, i) => `${k}: ${row[i]}`), ...head.slice(1), ...lines.slice(end)];
    writeFileSync(file, body.join("\n").replace(/\n*$/, "\n"));
    console.log(`check-goals: ${id} moved from this branch's table into docs/goals/${id}.md`);
    moved++;
  }
  git(["checkout", "MERGE_HEAD", "--", "docs/goals.md"]);
  console.log(`check-goals: ${moved} row(s) moved; docs/goals.md taken from main. Re-apply any prose edit it lost, then git add docs/goals.md docs/goals/`);
}

const goals = readFileSync(join(ROOT, "docs/goals.md"), "utf8").split("\n");
const table = goals.findIndex((l) => l.startsWith("| # | Milestone |"));
if (table >= 0) {
  err(
    `docs/goals.md:${table + 1}: a milestone table; rows live in docs/goals/<row>.md, one file each ` +
      "(Status, Milestone, Exit and Labs lines). Merging main into an older branch: run " +
      "`node tools/check-goals.mjs --absorb` mid-merge, which moves this branch's row edits into the files",
  );
}

const rows = [];
for (const f of existsSync(DIR) ? readdirSync(DIR).sort() : []) {
  if (!f.endsWith(".md") || FREE.has(f)) continue;
  const id = f.slice(0, -3);
  const at = `docs/goals/${f}`;
  const text = readFileSync(join(DIR, f), "utf8");
  const first = text.split("\n")[0];
  const m = /^Status: (.*)$/.exec(first);
  if (!m || !WORDS.includes(m[1])) {
    err(`${at}:1: the first line is \`${first}\`; make it \`Status: done\`, \`Status: in progress\` or \`Status: open\``);
    continue;
  }
  const h = header(text);
  const missing = KEYS.filter((k) => !h.get(k));
  if (missing.length) {
    err(
      `${at}: the header (the lines before the first blank one) has no ${missing.map((k) => `\`${k}:\``).join(", ")} ` +
        "line; add it under `Status:` (docs/goals.md § Milestones shows the shape)",
    );
    continue;
  }
  rows.push({ id, status: m[1], ...Object.fromEntries(KEYS.map((k) => [k, h.get(k)])) });
}
rows.sort((a, b) => byId(a.id, b.id));

if (errors.length) {
  for (const e of errors) console.error(`check-goals: ${e}`);
  console.error(`check-goals: ${errors.length} problem(s). Fix each as it says (docs/goals.md § Milestones).`);
  process.exit(1);
}

// A table cell keeps its text on one line and its pipes escaped outside backticks.
const cell = (s) => s.replace(/\|/g, "\\|");
const out = [
  "<!-- GENERATED by tools/check-goals.mjs (make index) from docs/goals/<row>.md; never edit or commit -->",
  "# Roadmap",
  "",
  "Every milestone row of [`goals.md`](goals.md), one file each in [`goals/`](goals/), in id order.",
  "",
  "| # | Milestone | Exit criterion | Labs it lands | Status |",
  "| --- | --- | --- | --- | --- |",
  ...rows.map(
    (r) => `| ${r.id} | ${cell(r.Milestone)} | ${cell(r.Exit)} | ${cell(r.Labs)} | [${r.status}](goals/${r.id}.md) |`,
  ),
  "",
];
writeFileSync(join(ROOT, "docs/roadmap.md"), out.join("\n"));
const count = (w) => rows.filter((r) => r.status === w).length;
console.log(
  `check-goals: ok (${rows.length} rows: ${count("done")} done, ${count("in progress")} in progress, ` +
    `${count("open")} open; table written to docs/roadmap.md)`,
);

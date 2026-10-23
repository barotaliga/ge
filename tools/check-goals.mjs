// THE ROADMAP's check (docs/goals.md § Milestones; run by `make agents-check`). Each milestone
// row's Status lives in its own file, docs/goals/<row>.md, so threads updating different rows
// never touch the same lines: the table's Status cell is the status word linking that file, and
// the file's first line says the same word. Holds:
//   - every row of the milestone table has docs/goals/<row>.md, and every such file a row;
//   - the file's first line is `Status: done`, `Status: in progress` or `Status: open`;
//   - the row's Status cell is exactly `[<word>](goals/<row>.md)` with the file's word.
// docs/goals/ge-backlog.md (the `ge` wishes, one a line) belongs to no row.
//
// ge:summary Checks the roadmap: each milestone row's status file exists and agrees with the table
// ge:keywords goals roadmap milestones status check table status-file
// ge:skill write-code
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";

const ROOT = new URL("..", import.meta.url).pathname;
const WORDS = ["done", "in progress", "open"];
const FREE = new Set(["ge-backlog.md"]);
const errors = [];
const err = (msg) => errors.push(msg);

const goals = readFileSync(join(ROOT, "docs/goals.md"), "utf8").split("\n");
const start = goals.findIndex((l) => l.startsWith("| # | Milestone |"));
if (start < 0) err("docs/goals.md: no milestone table (a row starting `| # | Milestone |`)");

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

const rows = new Set();
for (let k = start + 2; start >= 0 && k < goals.length && goals[k].startsWith("| "); k++) {
  const c = cells(goals[k]);
  const id = c[0];
  rows.add(id);
  const at = `docs/goals.md:${k + 1}`;
  const file = join(ROOT, "docs/goals", `${id}.md`);
  if (!existsSync(file)) {
    err(`${at}: row ${id} has no status file; write docs/goals/${id}.md, its first line \`Status: open\``);
    continue;
  }
  const first = readFileSync(file, "utf8").split("\n")[0];
  const m = /^Status: (.*)$/.exec(first);
  if (!m || !WORDS.includes(m[1])) {
    err(`docs/goals/${id}.md:1: the first line is \`${first}\`; make it \`Status: done\`, \`Status: in progress\` or \`Status: open\``);
    continue;
  }
  const want = `[${m[1]}](goals/${id}.md)`;
  if (c[c.length - 1] !== want) {
    err(`${at}: row ${id}'s Status cell is \`${c[c.length - 1]}\`; docs/goals/${id}.md says ${m[1]}, so make the cell \`${want}\``);
  }
}
for (const f of existsSync(join(ROOT, "docs/goals")) ? readdirSync(join(ROOT, "docs/goals")) : []) {
  if (!f.endsWith(".md") || FREE.has(f)) continue;
  if (!rows.has(f.slice(0, -3))) {
    err(`docs/goals/${f}: no milestone row ${f.slice(0, -3)} in docs/goals.md; add the row or delete the file`);
  }
}

if (errors.length) {
  for (const e of errors) console.error(`check-goals: ${e}`);
  console.error(`check-goals: ${errors.length} problem(s). Fix each as it says (docs/goals.md § Milestones).`);
  process.exit(1);
}
console.log(`check-goals: ok (${rows.size} rows, each with its status file)`);

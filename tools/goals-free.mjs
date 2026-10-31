#!/usr/bin/env node
// `ge goals free` and `ge goals claim` without a build (engine/cli/goals.hpp is the built one):
// for a fresh session whose tree is not built yet. The same rules: the rows main holds (each
// docs/goals/<row>.md, the table rows of docs/goals.md and docs/goals/reserved.md), every remote
// branch's and the working tree's; the next free number, or the next letter (or digit) under a
// row; a claim writes the row file, commits it alone, pushes it to this branch and checks again.
//
//   node tools/goals-free.mjs [<row>] [--base=origin/main] [--no-fetch]   the next free row
//   node tools/goals-free.mjs --claim "<what>" [--under=<row>] [--exit=<text>] [--labs=a,b]
//
// A claim that finds the same row on another branch says so and stops: the earlier claim keeps
// it, so drop yours (git revert) and claim again. `ge goals claim` settles that on its own.
//
// ge:summary ge goals free and claim in Node, for a tree with no build yet
// ge:keywords goals free claim roadmap row next-free no-build fallback node branches reserved
// ge:skill start-work
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const ROOT = new URL("..", import.meta.url).pathname;
const git = (...args) => {
  try {
    return execFileSync("git", ["-C", ROOT, ...args], {
      encoding: "utf8",
      stdio: ["ignore", "pipe", "ignore"],
      maxBuffer: 64 << 20,
    });
  } catch {
    return null;
  }
};

const args = process.argv.slice(2);
const flag = (name) => {
  const a = args.find((x) => x.startsWith(`--${name}=`));
  return a ? a.slice(name.length + 3) : "";
};
const words = args.filter((x) => !x.startsWith("--"));
const base = flag("base") || "origin/main";

// "M18k1" -> { number: 18, suffix: "k1" }; null when it is no row.
const rowId = (s) => {
  const m = /^M(\d+)([A-Za-z0-9.]*)/.exec(s);
  return m ? { number: Number(m[1]), suffix: m[2] } : null;
};
const tableRows = (md) =>
  md
    .split("\n")
    .map((l) => /^\|\s*\**(M\d[A-Za-z0-9.]*)\**\s*\|/.exec(l))
    .filter(Boolean)
    .map((m) => rowId(m[1]));
const fileRows = (names) =>
  names
    .map((n) => n.replace(/^.*\//, ""))
    .filter((n) => n.endsWith(".md"))
    .map((n) => rowId(n.slice(0, -3)))
    .filter((r) => r && `M${r.number}${r.suffix}` !== "M");

const rowsAt = (ref) => {
  const rows = [];
  for (const f of ["docs/goals.md", "docs/goals/reserved.md"]) {
    const md = git("show", `${ref}:${f}`);
    if (md) rows.push(...tableRows(md));
  }
  const ls = git("ls-tree", "--name-only", ref, "docs/goals/");
  if (ls) rows.push(...fileRows(ls.split("\n")));
  return rows;
};
const rowsHere = () => {
  const rows = [];
  for (const f of ["docs/goals.md", "docs/goals/reserved.md"]) {
    if (existsSync(join(ROOT, f))) rows.push(...tableRows(readFileSync(join(ROOT, f), "utf8")));
  }
  rows.push(...fileRows(readdirSync(join(ROOT, "docs/goals"))));
  return rows;
};

// The next free row (top level, or under `under`) and who holds rows main does not.
const scan = (under) => {
  const parent = under ? rowId(under) : null;
  if (under && !parent) throw new Error(`"${under}" is not a milestone row; name one as M17`);
  const sources = [{ name: base, rows: rowsAt(base) }];
  for (const ref of (git("for-each-ref", "--format=%(refname:short)", "refs/remotes/origin") || "")
    .split("\n")
    .map((r) => r.trim())) {
    if (!ref || ref === base || ref === "origin/HEAD" || ref === "origin") continue;
    sources.push({ name: ref, rows: rowsAt(ref) });
  }
  sources.push({ name: "the working tree", rows: rowsHere() });
  const key = (r) => `M${r.number}${r.suffix}`;
  const onBase = new Set(sources[0].rows.map(key));
  const baseMost = Math.max(-1, ...sources[0].rows.map((r) => r.number));
  const numbers = [];
  const tails = [];
  const claims = new Map();
  for (const s of sources) {
    for (const r of s.rows) {
      if (parent && r.number !== parent.number) continue;
      if (parent && (r.suffix.length <= parent.suffix.length || !r.suffix.startsWith(parent.suffix)))
        continue;
      numbers.push(r.number);
      if (parent) tails.push(r.suffix.slice(parent.suffix.length));
      if (onBase.has(key(r)) || (!parent && (r.suffix || r.number <= baseMost))) continue;
      const who = claims.get(key(r)) || [];
      if (!who.includes(s.name)) who.push(s.name);
      claims.set(key(r), who);
    }
  }
  let next;
  if (!parent) {
    next = `M${Math.max(-1, ...numbers) + 1}`;
  } else if (/[a-z]$/i.test(parent.suffix)) {
    const most = Math.max(0, ...tails.map((t) => Number(/^\d{1,4}/.exec(t)?.[0] || 0)));
    next = `${under}${most + 1}`;
  } else {
    const letters = tails.map((t) => t[0]).filter((c) => c >= "a" && c <= "z");
    const most = letters.sort().pop();
    if (most === "z") throw new Error(`no letter is free under ${under}; claim a top-level row`);
    next = `${under}${most ? String.fromCharCode(most.charCodeAt(0) + 1) : "a"}`;
  }
  return { next, claims, branches: sources.length - 2 };
};

const rowFile = (title, exit, labs) => {
  const cell = labs
    .split(",")
    .map((l) => l.trim())
    .filter(Boolean)
    .map((l) => `\`${l}\``)
    .join(", ");
  return `Status: open\nMilestone: **${title}**\nExit: ${exit || "to be written"}\nLabs: ${
    cell || "to be written"
  }\n\nNotes to be written.\n`;
};

try {
  if (!args.includes("--no-fetch") && git("fetch", "--quiet", "origin") === null)
    console.error("goals-free: git fetch origin failed; reading the branches as they were");
  if (!args.includes("--claim")) {
    const s = scan(words[0] || "");
    for (const [row, who] of [...s.claims].sort())
      console.log(`  ${row} held by ${who.join(", ")}`);
    console.log(
      `${s.next} is free (${s.claims.size} rows held on branches only, ${s.branches} branches read)`,
    );
    console.log(`next: node tools/goals-free.mjs --claim "<what the row is>"`);
    process.exit(0);
  }
  const title = words.join(" ");
  const exit = flag("exit");
  const labs = flag("labs");
  if (!title) throw new Error('name the row: --claim "<what the row is>"');
  if (/[\r\n]/.test(title + exit + labs)) throw new Error("--exit and --labs are one line each");
  const branch = (git("rev-parse", "--abbrev-ref", "HEAD") || "").trim();
  if (!branch || branch === "HEAD") throw new Error("HEAD is detached; check out your branch");
  const { next } = scan(flag("under"));
  const path = `docs/goals/${next}.md`;
  writeFileSync(join(ROOT, path), rowFile(title, exit, labs));
  if (git("add", "--", path) === null || git("commit", "-m", `docs(goals): reserve ${next} for ${title}`, "--", path) === null)
    throw new Error(`git commit of ${path} failed`);
  if (git("push", "--quiet", "origin", `HEAD:refs/heads/${branch}`) === null)
    throw new Error(`the push to ${branch} was refused; the claim's commit is on HEAD`);
  git("fetch", "--quiet", "origin");
  const others = (scan(flag("under")).claims.get(next) || []).filter(
    (w) => w !== `origin/${branch}` && w !== "the working tree",
  );
  if (others.length) {
    console.error(`goals-free: ${next} is also held by ${others.join(", ")}; the earlier claim keeps it.`);
    console.error("  Revert your claim's commit, push, and claim again (or use `ge goals claim`).");
    process.exit(1);
  }
  console.log(`claimed ${next} (${path}), pushed to ${branch}`);
} catch (e) {
  console.error(`goals-free: ${e.message}`);
  process.exit(2);
}

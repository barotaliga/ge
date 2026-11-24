#!/usr/bin/env node
// THE LEDGERS' COUNTS, counted from their rows (a WORKING file: porting/ is deleted when porting
// ends). No ledger keeps a total by hand: hand totals went stale and conflicted in every pull
// request that touched a ledger. This counts every row of each ledger by its table's `status`
// column (done, verify, missing, not-taken, partial), section by section (`## ` headings), the
// missing rows by rank (the `rank` column, else "rank N" in the row; none: unranked).
//
//   node porting/ledger-count.mjs                 every ledger's sections and totals
//   node porting/ledger-count.mjs --missing       one line a ledger: its missing count by rank
//   node porting/ledger-count.mjs <file.md> ...   those ledgers only
//   node porting/ledger-count.mjs --json          the counts as one JSON object
//
// `make ledger` (porting/porting.mk) runs it; ARGS pass on. A ledger is any
// porting/shooters-edge/*ledger*.md, and any other file named on the command line.
//
// ge:summary The porting ledgers' counts from their rows: by section, missing by rank; make ledger
// ge:keywords ledger missing-count ledger-count make-ledger totals rank port-from-ledger
// ge:skill port-from-ledger
import fs from 'node:fs';
import path from 'node:path';

const STATUSES = ['done', 'verify', 'partial', 'missing', 'not-taken'];
const here = path.dirname(new URL(import.meta.url).pathname);

function cells(line) {
  // Split on bars not escaped with a backslash; drop the outer empty cells.
  return line.split(/(?<!\\)\|/).slice(1, -1).map((c) => c.trim());
}

function blank() {
  return { rows: 0, done: 0, verify: 0, partial: 0, missing: 0, rank: [0, 0, 0, 0], 'not-taken': 0 };
}

function add(into, from) {
  for (const k of ['rows', 'done', 'verify', 'partial', 'missing', 'not-taken']) into[k] += from[k];
  for (let i = 0; i < 4; i++) into.rank[i] += from.rank[i];
}

export function countLedger(text) {
  const sections = [];
  let section = null;
  let statusCol = -1;
  let rankCol = -1;
  for (const raw of text.split('\n')) {
    const line = raw.trimEnd();
    if (line.startsWith('## ')) {
      section = { name: line.slice(3).trim(), ...blank() };
      sections.push(section);
      statusCol = rankCol = -1;
      continue;
    }
    if (!line.startsWith('|')) continue;
    const c = cells(line);
    if (c.length === 0) continue;
    const lower = c.map((x) => x.toLowerCase());
    if (lower.includes('status')) {  // a table's header row
      statusCol = lower.indexOf('status');
      rankCol = lower.indexOf('rank');
      continue;
    }
    if (statusCol < 0 || c.every((x) => /^:?-+:?$/.test(x))) continue;
    let status = (c[statusCol] || '').toLowerCase().replace(/^not taken$/, 'not-taken');
    status = status.split(/[\s,(]/)[0];
    if (!STATUSES.includes(status)) continue;
    if (!section) {
      section = { name: '(before the first section)', ...blank() };
      sections.push(section);
    }
    section.rows++;
    section[status]++;
    if (status === 'missing') {
      let rank = rankCol >= 0 ? parseInt(c[rankCol], 10) : NaN;
      if (!(rank >= 1 && rank <= 3)) {
        const m = /\brank\s*([123])\b/i.exec(line);
        rank = m ? parseInt(m[1], 10) : 0;
      }
      section.rank[rank >= 1 && rank <= 3 ? rank - 1 : 3]++;
    }
  }
  const total = blank();
  for (const s of sections) add(total, s);
  return { sections: sections.filter((s) => s.rows > 0), total };
}

function missingText(t) {
  const [a, b, c, none] = t.rank;
  return `${t.missing} (${a} / ${b} / ${c}${none ? ` / ${none} unranked` : ''})`;
}

function line(name, t) {
  const parts = [`${t.rows} rows`, `done ${t.done}`];
  if (t.verify) parts.push(`verify ${t.verify}`);
  if (t.partial) parts.push(`partial ${t.partial}`);
  parts.push(`missing ${missingText(t)}`, `not-taken ${t['not-taken']}`);
  return `${name}: ${parts.join(', ')}`;
}

function main(argv) {
  const files = argv.filter((a) => !a.startsWith('--'));
  const flags = new Set(argv.filter((a) => a.startsWith('--')));
  for (const f of flags) {
    if (!['--missing', '--json'].includes(f)) {
      console.error(`ledger-count: unknown flag ${f}; use --missing or --json (or name ledger files)`);
      process.exit(2);
    }
  }
  const dir = path.join(here, 'shooters-edge');
  const list = files.length
    ? files
    : fs.readdirSync(dir).filter((f) => /ledger.*\.md$/.test(f)).sort().map((f) => path.join(dir, f));
  const out = {};
  for (const f of list) {
    if (!fs.existsSync(f)) {
      console.error(`ledger-count: ${f} does not exist; name a ledger under porting/shooters-edge/`);
      process.exit(2);
    }
    out[path.basename(f, '.md')] = countLedger(fs.readFileSync(f, 'utf8'));
  }
  if (flags.has('--json')) {
    console.log(JSON.stringify(out, null, 2));
    return;
  }
  for (const [name, r] of Object.entries(out)) {
    if (flags.has('--missing')) {
      console.log(`${name}: ${missingText(r.total)} missing`);
      continue;
    }
    console.log(line(name, r.total));
    for (const s of r.sections) console.log('  ' + line(s.name, s));
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === new URL(import.meta.url).pathname)
  main(process.argv.slice(2));

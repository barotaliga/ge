// The guard every golden script that reaches a transcendental runs first: it must execute
// under game5's pinned Node major (../game5/.nvmrc) on Linux x86-64 with glibc, because V8's
// Math functions changed between majors and Math.pow in Node 24 is the host libm
// (docs/research.md section 2). Exits with the fix when it is anything else; returns the
// provenance banner the golden file's first line carries.
//
// ge:summary Refuses a golden run not under game5's pinned Node on Linux x86-64 glibc
// ge:keywords oracle node24 nvmrc glibc guard provenance
// ge:skill testing
import { readFileSync } from "node:fs";
import { resolve } from "node:path";

export function requireOracleNode(game5, script) {
  const pinned = readFileSync(resolve(game5, ".nvmrc"), "utf8").trim().replace(/^v/, "");
  const major = process.versions.node.split(".")[0];
  const glibc = process.report?.getReport().header.glibcVersionRuntime;
  if (major !== pinned.split(".")[0]) {
    console.error(
      `${script}: running Node ${process.versions.node}, but game5 pins Node ${pinned} ` +
        `(${game5}/.nvmrc). Install that major from https://nodejs.org/dist/ and rerun with ` +
        `make golden-libm NODE=<its bin/node>.`,
    );
    process.exit(1);
  }
  if (process.platform !== "linux" || process.arch !== "x64" || !glibc) {
    console.error(
      `${script}: Math.pow in Node ${major} is the host libm; game5's reference platform is ` +
        `Linux x86-64 with glibc, but this is ${process.platform}-${process.arch}` +
        `${glibc ? ` glibc ${glibc}` : ""}. Run it on Linux x86-64 (e.g. an ubuntu CI runner).`,
    );
    process.exit(1);
  }
  return `# node ${process.versions.node} v8 ${process.versions.v8} ${process.platform}-${process.arch} glibc ${glibc}`;
}

// THE PLUGIN CONTRACT's check (docs/architecture.md § Plugins; run by `make agents-check`).
// Reads each plugins/<name>/CMakeLists.txt's one `ge_add_plugin(<name> ENGINE ... PLUGINS ...)`
// call (and its APP list, the engine targets the plugin's app/ half links) and holds the tree to
// it:
//   - the name is one lower-case word, equal to the directory, and shadows no engine/ directory;
//   - every plugin has its skill at .agents/skills/<name>/, and every tagged file in the plugin
//     names that skill in its ge:skill line;
//   - every #include "<root>/..." in a plugin reaches only itself, an engine directory whose
//     target it lists under ENGINE, or a plugin it lists under PLUGINS (and from app/, an engine
//     directory whose target it lists under APP); never games/ or tools/;
//   - the PLUGINS graph has no cycle;
//   - nothing under engine/ includes a plugin, and no plugin's name is a games/ directory.
//
// ge:summary Checks the plugin contract: names, skills, declared dependencies, no cycles
// ge:keywords plugin plugins check depends include direction engine-plugin-game ge_add_plugin
// ge:skill plugins write-code
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";

const ROOT = new URL("..", import.meta.url).pathname;
const errors = [];
const err = (msg) => errors.push(msg);
const dirs = (p) =>
  existsSync(join(ROOT, p))
    ? readdirSync(join(ROOT, p), { withFileTypes: true }).filter((d) => d.isDirectory()).map((d) => d.name)
    : [];
const files = (p) =>
  execFileSync("git", ["ls-files", "--cached", "--others", "--exclude-standard", p], { cwd: ROOT, encoding: "utf8" })
    .split("\n")
    .filter((f) => f && existsSync(join(ROOT, f)));
const includes = (f) => [...readFileSync(join(ROOT, f), "utf8").matchAll(/^\s*#\s*include\s+"([^"/]+)\//gm)].map((m) => m[1]);

// Which engine target each engine/ directory is built into: each engine/<module>/module.cmake's
// ge_module(ge_<target> [DIRS <dir>...]) calls (cmake/Modules.cmake), the file's own directory
// when it names none.
const ENGINE_TARGET = {};
const engineDirs = dirs("engine");
for (const d of engineDirs) {
  const mc = join("engine", d, "module.cmake");
  if (!existsSync(join(ROOT, mc))) continue;
  const text = readFileSync(join(ROOT, mc), "utf8").replace(/#.*$/gm, "");
  for (const call of text.matchAll(/ge_module\s*\(([^)]*)\)/g)) {
    const words = call[1].split(/\s+/).filter(Boolean);
    const target = words[0].replace(/^ge_/, "");
    const at = words.indexOf("DIRS");
    const owned = [];
    if (at >= 0) for (const w of words.slice(at + 1)) { if (/^[A-Z_]+$/.test(w)) break; owned.push(w); }
    for (const o of owned.length ? owned : [d]) ENGINE_TARGET[o] ??= target;
  }
}
for (const d of engineDirs) {
  if (!ENGINE_TARGET[d]) err(`engine/${d}/ is built into no target: add engine/${d}/module.cmake ending in ge_module(ge_${d} ...), or name it in another module's DIRS (cmake/Modules.cmake).`);
}

const plugins = new Map();
for (const name of dirs("plugins")) {
  const cm = join("plugins", name, "CMakeLists.txt");
  if (!existsSync(join(ROOT, cm))) {
    err(`plugins/${name}/ has no CMakeLists.txt. Add one ge_add_plugin(${name} ENGINE core) call (skill: plugins).`);
    continue;
  }
  const call = readFileSync(join(ROOT, cm), "utf8").replace(/#.*$/gm, "").match(/ge_add_plugin\s*\(([^)]*)\)/);
  if (!call) {
    err(`${cm} has no ge_add_plugin(...) call. A plugin's CMakeLists.txt is that one call (skill: plugins).`);
    continue;
  }
  const words = call[1].split(/\s+/).filter(Boolean);
  const decl = { name: words[0], engine: [], plugins: [], app: [] };
  let list = null;
  for (const w of words.slice(1)) {
    if (w === "ENGINE") list = decl.engine;
    else if (w === "PLUGINS") list = decl.plugins;
    else if (w === "APP") list = decl.app;
    else if (list) list.push(w);
    else err(`${cm}: '${w}' is outside an ENGINE, PLUGINS or APP list.`);
  }
  if (decl.name !== name) err(`${cm} declares '${decl.name}'; a plugin's name is its directory's ('${name}').`);
  if (!/^[a-z][a-z0-9]*$/.test(name)) err(`plugins/${name}: a plugin name is one lower-case word (skill: plugins).`);
  if (engineDirs.includes(name)) err(`plugins/${name} shadows engine/${name}/: "${name}/x.hpp" would be ambiguous. Rename the plugin.`);
  if (existsSync(join(ROOT, "games", name))) err(`plugins/${name} has the same name as games/${name}/. Rename the plugin.`);
  plugins.set(name, decl);
}

for (const [name, decl] of plugins) {
  const engineTargets = new Set(Object.values(ENGINE_TARGET));
  for (const t of [...decl.engine, ...decl.app]) {
    if (!engineTargets.has(t)) err(`plugins/${name} lists ENGINE ${t}, which is not an engine target (${[...engineTargets].join(", ")}).`);
  }
  for (const p of decl.plugins) {
    if (!plugins.has(p)) err(`plugins/${name} lists PLUGINS ${p}, which is not a directory in plugins/.`);
  }
  if (!existsSync(join(ROOT, ".agents/skills", name, "SKILL.md"))) {
    err(`plugins/${name} has no skill: add .agents/skills/${name}/SKILL.md (skill: plugins, § Add a plugin).`);
  }
  const allowed = new Set([name, ...decl.plugins, ...engineDirs.filter((d) => decl.engine.includes(ENGINE_TARGET[d]))]);
  const appAllowed = new Set([...allowed, ...engineDirs.filter((d) => decl.app.includes(ENGINE_TARGET[d]))]);
  for (const f of files(`plugins/${name}`)) {
    if (/\.(cpp|hpp|h)$/.test(f)) {
      // A plugin's labs/ is built into the `labs` executable, not the plugin (docs/labs.md), so
      // it may also include the lab framework; its assets/ (a model's data set, engine/assets/
      // data.hpp) into the `assets` executable, so it may include the assets library.
      const isLab = f.startsWith(`plugins/${name}/labs/`);
      const isAsset = f.startsWith(`plugins/${name}/assets/`);
      const isApp = f.startsWith(`plugins/${name}/app/`);
      const may = isApp || isLab || isAsset ? appAllowed : allowed;
      for (const root of includes(f)) {
        if (!may.has(root) && !(isLab && root === "lab") && !(isAsset && root === "assets")) {
          err(`${f} includes "${root}/...", which plugins/${name} does not declare. Add it to ENGINE, ` +
            `PLUGINS or (for app/) APP in plugins/${name}/CMakeLists.txt if the dependency is right; a plugin never ` +
            `includes a game (skill: plugins).`);
        }
      }
      const skills = readFileSync(join(ROOT, f), "utf8").split("\n", 60).filter((l) => /ge:skill\s/.test(l)).join(" ");
      if (skills && !new RegExp(`ge:skill\\b.*\\b${name}\\b`).test(skills)) {
        err(`${f}: ge:skill must name the plugin's skill '${name}' (first), so make find and the index route here.`);
      }
    }
  }
}

// No cycle in the PLUGINS graph (depth-first, three colours).
const state = new Map();
const visit = (n, path) => {
  if (state.get(n) === 1) return err(`plugin dependency cycle: ${[...path, n].join(" -> ")}. Move the shared part into a plugin both depend on.`);
  if (state.get(n) === 2 || !plugins.has(n)) return;
  state.set(n, 1);
  for (const d of plugins.get(n).plugins) visit(d, [...path, n]);
  state.set(n, 2);
};
for (const n of plugins.keys()) visit(n, []);

// The engine never reaches up into a plugin.
for (const f of files("engine")) {
  if (!/\.(cpp|hpp|h)$/.test(f)) continue;
  for (const root of includes(f)) {
    if (plugins.has(root)) err(`${f} includes the plugin "${root}/". The engine never depends on a plugin: move the code it needs into engine/ or the caller into the plugin.`);
  }
}

if (errors.length) {
  for (const e of errors) console.error(`check-plugins: ${e}`);
  process.exit(1);
}
console.log(`check-plugins: ok (${plugins.size} plugins)`);

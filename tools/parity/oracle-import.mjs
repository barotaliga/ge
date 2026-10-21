// Imports an oss-game-framework source file (`src/<path>.ts`) straight into Node, with types
// stripped (Node 22.6+ flag, the default from 23.6). The framework writes its relative imports
// without an extension (`from "./math"`), which tsup resolves and Node does not, so a resolve
// hook retries a failed relative import with `.ts` appended.
//
// ge:summary Imports a framework .ts source into Node with types stripped
// ge:keywords oracle import loader strip-types resolve-hook
// ge:skill testing
import { registerHooks } from "node:module";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

registerHooks({
  resolve(specifier, context, next) {
    try {
      return next(specifier, context);
    } catch (err) {
      if (err?.code !== "ERR_MODULE_NOT_FOUND" || !specifier.startsWith(".")) throw err;
      return next(`${specifier}.ts`, context);
    }
  },
});

export const oracleLoader = (framework) => (path) =>
  import(pathToFileURL(resolve(framework, path)).href);

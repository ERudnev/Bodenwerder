import * as esbuild from "esbuild";
import { spawnSync } from "child_process";
import { fileURLToPath } from "url";
import { dirname, join } from "path";

const dir = dirname(fileURLToPath(import.meta.url));
const out = join(dir, "smoke-out.cjs");

await esbuild.build({
  entryPoints: [join(dir, "smoke-entry.ts")],
  bundle: true,
  outfile: out,
  platform: "node",
  format: "cjs",
});

const result = spawnSync(process.execPath, [out], { stdio: "inherit" });
process.exit(result.status ?? 1);

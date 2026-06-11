import { readFileSync } from "fs";
import { join } from "path";
import { TomeParser } from "../parser/tome-parser";

const repoRoot = join(__dirname, "..", "..", "..");
const files = [
  "_seed/methods/management.tome",
  "_seed/contract.tome",
  "_seed/methods/processes.tome",
  "_seed/_management/plan.tome",
  "_seed/core/KQM.tome",
];

const parser = new TomeParser();
let failed = false;

for (const rel of files) {
  const path = join(repoRoot, rel);
  const source = readFileSync(path, "utf8");
  const doc = parser.parse(source);
  const ns = doc.nodes.find((n) => n.kind === "namespace");
  const artifactCount = ns?.kind === "namespace" ? ns.artifacts.length : 0;
  const usingCount = doc.nodes.filter((n) => n.kind === "using").length;

  if (artifactCount === 0) {
    console.error(`FAIL ${rel}: no artifacts parsed`);
    failed = true;
    continue;
  }

  console.log(`OK ${rel}: usings=${usingCount}, artifacts=${artifactCount}`);
}

if (failed) {
  process.exit(1);
}

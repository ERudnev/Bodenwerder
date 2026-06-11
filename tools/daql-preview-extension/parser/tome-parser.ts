import type { Artifact, Document, TopLevelNode } from "../model/document";
import type { Parser } from "./index";
import {
  extractBraceBlock,
  parseArtifactFromLines,
  parseArtifactHeader,
} from "./artifact";
import { parseLine } from "./inline";

const USING_LINE =
  /^#using\s+namespace\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*file:\s*"([^"]*)"\s*\)\s*$/;
const NAMESPACE_LINE = /^namespace\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{/;

function splitLines(source: string): string[] {
  return source.replace(/\r\n/g, "\n").split("\n");
}

function parseNamespaceBlock(
  lines: string[],
  startIndex: number,
): { name: string; artifacts: Artifact[]; endIndex: number } | null {
  const match = NAMESPACE_LINE.exec(lines[startIndex].trim());
  if (!match) {
    return null;
  }

  const name = match[1];
  let depth = 1;
  let i = startIndex + 1;

  while (i < lines.length && depth > 0) {
    const line = lines[i];
    for (let c = 0; c < line.length; c++) {
      if (line[c] === "{") {
        depth++;
      } else if (line[c] === "}") {
        depth--;
        if (depth === 0) {
          const artifacts = parseArtifactsInRange(lines, startIndex + 1, i);
          return { name, artifacts, endIndex: i + 1 };
        }
      }
    }
    i++;
  }

  return null;
}

function parseArtifactsInRange(lines: string[], start: number, end: number): Artifact[] {
  const artifacts: Artifact[] = [];
  let i = start;

  while (i < end) {
    const line = lines[i];
    if (line.trim() === "" || line.trim() === "}") {
      i++;
      continue;
    }

    if (!parseArtifactHeader(line)) {
      i++;
      continue;
    }

    const block = extractBraceBlock(lines, i);
    if (!block) {
      i++;
      continue;
    }

    const artifact = parseArtifactFromLines(line, block.bodyLines);
    if (artifact) {
      artifacts.push(artifact);
    }
    i = block.endIndex;
  }

  return artifacts;
}

function parseTopLevel(lines: string[]): TopLevelNode[] {
  const nodes: TopLevelNode[] = [];
  let i = 0;

  while (i < lines.length) {
    const line = lines[i];
    const trimmed = line.trim();

    if (trimmed === "") {
      i++;
      continue;
    }

    if (trimmed.startsWith("//")) {
      nodes.push({ kind: "comment", text: trimmed.slice(2).trimStart() });
      i++;
      continue;
    }

    const usingMatch = USING_LINE.exec(trimmed);
    if (usingMatch) {
      nodes.push({ kind: "using", namespace: usingMatch[1], file: usingMatch[2] });
      i++;
      continue;
    }

    if (NAMESPACE_LINE.test(trimmed)) {
      const ns = parseNamespaceBlock(lines, i);
      if (ns) {
        nodes.push({
          kind: "namespace",
          name: ns.name,
          artifacts: ns.artifacts,
        });
        i = ns.endIndex;
        continue;
      }
    }

    nodes.push({ kind: "prose", inlines: parseLine(trimmed) });
    i++;
  }

  return nodes;
}

export class TomeParser implements Parser {
  parse(source: string): Document {
    const lines = splitLines(source);
    return { nodes: parseTopLevel(lines) };
  }
}

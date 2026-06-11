import type { Artifact, ArtifactBody, FormExpr, Section } from "../model/document";
import { parseFormExpr } from "./form-expr";
import { parseLine } from "./inline";
import { parseRuleTree } from "./rule-tree";

const ARTIFACT_HEADER = /^(\s*)([A-Za-z_][A-Za-z0-9_]*):\s*(.+)$/;
const SECTION_LABEL = /^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$/;

export function parseArtifactHeader(line: string): { name: string; form: FormExpr } | null {
  const match = ARTIFACT_HEADER.exec(line);
  if (!match) {
    return null;
  }

  const rest = match[3].trim();
  const braceIdx = rest.lastIndexOf("{");
  if (braceIdx === -1) {
    return null;
  }

  const formPart = rest.slice(0, braceIdx).trim();
  const form = parseFormExpr(formPart);
  if (!form) {
    return null;
  }

  return { name: match[2], form };
}

export function parseArtifactBody(
  lines: string[],
  form: FormExpr,
): ArtifactBody {
  const trimmed = lines.map((l) => l.trimEnd());

  if (form.name === "Rule" && !hasSectionLabels(trimmed)) {
    const { prologue, items } = parseRuleTree(trimmed);
    return { kind: "rule", prologue, items };
  }

  const sections = parseSections(trimmed);
  if (form.name === "Rule" && sections.length === 0) {
    const { prologue, items } = parseRuleTree(trimmed);
    return { kind: "rule", prologue, items };
  }

  return { kind: "sections", sections };
}

function hasSectionLabels(lines: string[]): boolean {
  const known = new Set([
    "Clause",
    "Notation",
    "Examples",
    "Core",
    "Reason",
    "status",
    "goal",
    "subtasks",
    "reason",
    "dependencies",
  ]);
  for (const line of lines) {
    const trimmed = line.trim();
    const match = SECTION_LABEL.exec(trimmed);
    if (match && known.has(match[1])) {
      return true;
    }
  }
  return false;
}

function parseSections(lines: string[]): Section[] {
  const sections: Section[] = [];
  let current: Section | null = null;

  for (const line of lines) {
    const trimmed = line.trim();
    if (trimmed === "") {
      continue;
    }

    const labelMatch = SECTION_LABEL.exec(trimmed);
    if (labelMatch) {
      if (current) {
        sections.push(current);
      }
      current = {
        label: labelMatch[1],
        lines: labelMatch[2] ? [parseLine(labelMatch[2])] : [],
      };
      continue;
    }

    if (current) {
      current.lines.push(parseLine(trimmed));
    }
  }

  if (current) {
    sections.push(current);
  }

  return sections;
}

export function parseArtifactFromLines(
  headerLine: string,
  bodyLines: string[],
): Artifact | null {
  const header = parseArtifactHeader(headerLine);
  if (!header) {
    return null;
  }

  return {
    name: header.name,
    form: header.form,
    body: parseArtifactBody(bodyLines, header.form),
  };
}

export function extractBraceBlock(
  lines: string[],
  startIndex: number,
): { bodyLines: string[]; endIndex: number } | null {
  const openLine = lines[startIndex];
  const openBrace = openLine.lastIndexOf("{");
  if (openBrace === -1) {
    return null;
  }

  const bodyLines: string[] = [];
  const afterBrace = openLine.slice(openBrace + 1).trim();
  if (afterBrace) {
    bodyLines.push(afterBrace);
  }

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
          const beforeClose = line.slice(0, c).trim();
          if (beforeClose) {
            bodyLines.push(beforeClose);
          }
          return { bodyLines, endIndex: i + 1 };
        }
      }
    }
    if (depth > 0) {
      bodyLines.push(line);
    }
    i++;
  }

  return { bodyLines, endIndex: i };
}

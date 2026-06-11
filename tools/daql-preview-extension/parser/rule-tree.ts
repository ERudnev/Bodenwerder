import type { Inline, RuleItem } from "../model/document";
import { parseLine } from "./inline";

const RULE_LINE = /^(\s*)(\d+):\s*(.*)$/;

export function parseRuleTree(lines: string[]): { prologue: Inline[][]; items: RuleItem[] } {
  const prologue: Inline[][] = [];
  const flatItems: { indent: number; index: string; text: Inline[] }[] = [];

  for (const line of lines) {
    if (line.trim() === "") {
      continue;
    }

    const match = RULE_LINE.exec(line);
    if (match) {
      flatItems.push({
        indent: match[1].length,
        index: match[2],
        text: parseLine(match[3]),
      });
    } else {
      prologue.push(parseLine(line));
    }
  }

  return { prologue, items: buildTree(flatItems, 0, flatItems.length, -1) };
}

function buildTree(
  flat: { indent: number; index: string; text: Inline[] }[],
  start: number,
  end: number,
  parentIndent: number,
): RuleItem[] {
  const items: RuleItem[] = [];
  let i = start;

  while (i < end) {
    const current = flat[i];
    if (current.indent <= parentIndent) {
      break;
    }

    let j = i + 1;
    while (j < end && flat[j].indent > current.indent) {
      j++;
    }

    items.push({
      index: current.index,
      text: current.text,
      children: buildTree(flat, i + 1, j, current.indent),
    });

    i = j;
  }

  return items;
}

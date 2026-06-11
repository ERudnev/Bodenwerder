import type { Inline } from "../model/document";

const IDENT = /[A-Za-z_][A-Za-z0-9_]*/;

function isSpecialStart(ch: string): boolean {
  return ch === "@" || ch === "&" || ch === "`";
}

function readIdentifier(text: string, start: number): { name: string; end: number } | null {
  const match = IDENT.exec(text.slice(start));
  if (!match) {
    return null;
  }
  return { name: match[0], end: start + match[0].length };
}

export function parseInlines(text: string): Inline[] {
  const inlines: Inline[] = [];
  let i = 0;

  while (i < text.length) {
    const ch = text[i];

    if (ch === "`") {
      const close = text.indexOf("`", i + 1);
      if (close !== -1) {
        inlines.push({ kind: "code", value: text.slice(i + 1, close) });
        i = close + 1;
        continue;
      }
    }

    if (ch === "@") {
      const ident = readIdentifier(text, i + 1);
      if (ident) {
        let name = ident.name;
        let qualifier: string | undefined;
        let end = ident.end;

        if (text[end] === ":" && text[end + 1] === ":") {
          const qual = readIdentifier(text, end + 2);
          if (qual) {
            qualifier = name;
            name = qual.name;
            end = qual.end;
          }
        }

        inlines.push({ kind: "formRef", name, qualifier });
        i = end;
        continue;
      }
    }

    if (ch === "&") {
      const ident = readIdentifier(text, i + 1);
      if (ident) {
        inlines.push({ kind: "artifactRef", name: ident.name });
        i = ident.end;
        continue;
      }
    }

    let end = i + 1;
    while (end < text.length && !isSpecialStart(text[end])) {
      end++;
    }

    inlines.push({ kind: "text", value: text.slice(i, end) });
    i = end;
  }

  return inlines;
}

export function parseLine(text: string): Inline[] {
  return parseInlines(text);
}
